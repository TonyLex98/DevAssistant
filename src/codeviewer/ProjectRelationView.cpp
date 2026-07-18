#include "ProjectRelationView.h"
#include <QVBoxLayout>
#include <QStatusBar>
#include <QApplication>
#include <QJsonArray>
#include <QMessageBox>
#include <tree_sitter/api.h>

extern "C" const TSLanguage *tree_sitter_cpp();
extern "C" const TSLanguage *tree_sitter_python();   // <-- добавлено
extern "C" const TSLanguage *tree_sitter_dart();     // <-- добавлено


ProjectRelationView::ProjectRelationView(QWidget *parent): QGraphicsView(parent){
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setDragMode(QGraphicsView::ScrollHandDrag);

    m_toolbar = new QToolBar("Graph", this);
    addActions(m_toolbar->actions());
    m_toolbar->addAction("Rebuild", this, &ProjectRelationView::rebuildGraph);
    m_toolbar->addAction("Validate", this, &ProjectRelationView::validateLostEdges);
    m_toolbar->addAction("Auto-Layout", [this]{ m_layout->applyAutoLayout(m_scene, m_classItems.values()); saveLayout(); });

    m_layout = new GraphLayout(this);
    m_indexer = new ProjectIndexer(this);
    m_patcher = new CodePatcher(this);
    connect(m_indexer, &ProjectIndexer::graphReady, this, &ProjectRelationView::onGraphReady);
}

void ProjectRelationView::setProjectRoot(const QString &root){
    m_root = root;
    m_indexer->setRoot(root);
    loadLayout();
    rebuildGraph();
}

void ProjectRelationView::rebuildGraph(){
    // keep previous edge set to mark lost after rebuild
    m_prevEdgeKeys.clear();
    for(const auto &e : m_graph.edges){
        m_prevEdgeKeys.insert(edgeKey(e));
    }
    m_indexer->rebuild();
}

void ProjectRelationView::validateLostEdges(){
    // Re-index synchronously and diff
    SymbolGraph fresh = m_indexer->buildGraphSync();
    QSet<QString> freshEdges;
    for(const auto &e : fresh.edges) freshEdges.insert(edgeKey(e));

    for(auto *edge : m_edgeItems){
        QString from = edge->data(1).toString();
        QString to = edge->data(2).toString();
        int kind = edge->data(3).toInt();
        QString key = from+"->"+to+"|"+QString::number(kind);
        bool lost = !freshEdges.contains(key);
        edge->setLost(lost);
    }
}

void ProjectRelationView::wheelEvent(QWheelEvent *e){
    if(e->modifiers() & Qt::ControlModifier){
        const double scaleFactor = 1.0015; int num = e->angleDelta().y(); double s = std::pow(scaleFactor, num);
        scale(s, s); e->accept(); return;
    }
    QGraphicsView::wheelEvent(e);
}

void ProjectRelationView::onGraphReady(const SymbolGraph &g){
    m_graph = g;
    buildSceneFromGraph(g);
    // After rebuild, add ghost edges that were lost since previous graph
    QSet<QString> current;
    for(const auto &e : g.edges) current.insert(edgeKey(e));
    for(const QString &k : m_prevEdgeKeys){
        if(!current.contains(k)){
            // parse key back
            int p = k.indexOf("->"), q = k.indexOf("|");
            if(p>0 && q>p){
                QString fromSid = k.left(p);
                QString toSid = k.mid(p+2, q-p-2);
                int kind = k.mid(q+1).toInt();
                auto *ghost = new EdgeItem(fromSid, toSid, (EdgeKind)kind);
                ghost->setLost(true);
                ghost->setData(1, fromSid);
                ghost->setData(2, toSid);
                ghost->setData(3, kind);
                m_scene->addItem(ghost);
                m_edgeItems.push_back(ghost);
            }
        }
    }
    refreshEdges();
    saveLayout();
}

void ProjectRelationView::clearScene(){
    for(auto *e: m_edgeItems) m_scene->removeItem(e);
    m_edgeItems.clear();
    for(auto *m: m_methodItems){ m_scene->removeItem(m); }
    m_methodItems.clear();
    for(auto *c: m_classItems){ m_scene->removeItem(c); }
    m_classItems.clear();
    m_scene->clear();
}

QPointF ProjectRelationView::anchorForSid(const QString &sid) const{
    if(m_methodItems.contains(sid)) return m_methodItems.value(sid)->sceneBoundingRect().center();
    if(m_classItems.contains(sid)) return m_classItems.value(sid)->sceneBoundingRect().center();
    return QPointF(0,0);
}

void ProjectRelationView::buildSceneFromGraph(const SymbolGraph &g){
    clearScene();

    // 1) Classes
    for(const auto &n: g.nodes){
        if(n.kind==NodeKind::Class){
            auto *ci = new ClassItem(n);
            m_scene->addItem(ci);
            m_classItems.insert(n.sid.id, ci);
            connect(ci,&ClassItem::positionChanged,this,[this]{ saveLayout(); refreshEdges(); });
            // apply saved pos if any
            if(m_savedClassPos.contains(n.sid.id)) ci->setPos(m_savedClassPos.value(n.sid.id));
        }
    }
    // 2) Methods/Fields/Functions
    for(const auto &n: g.nodes){
        if(n.kind==NodeKind::Method || n.kind==NodeKind::Field || n.kind==NodeKind::Function){
            ClassItem* parent = m_classItems.value(n.parentSid, nullptr);
            auto *chip = new MethodChip(n, parent ? (QGraphicsItem*)parent : nullptr);
            if(parent) parent->addChildChip(chip); else m_scene->addItem(chip);
            m_methodItems.insert(n.sid.id, chip);
            connect(chip, &MethodChip::openRequested, this, &ProjectRelationView::openInEditor);
            connect(chip, &MethodChip::positionChanged, this, [this]{ saveLayout(); refreshEdges(); });


            // restore local pos if available
            if(parent && m_savedMethodLocalPos.contains(parent->sid())){
                auto &mmap = m_savedMethodLocalPos[parent->sid()];
                if(mmap.contains(n.sid.id)) chip->setPos(mmap.value(n.sid.id));
            }
        }
    }
    // 3) Edges
    for(const auto &e: g.edges){
        auto *edge = new EdgeItem(e.fromSid, e.toSid, e.kind);
        edge->setData(1, e.fromSid);
        edge->setData(2, e.toSid);
        edge->setData(3, (int)e.kind);
        edge->setLabel(e.label);
        edge->setOrder(e.order);
        m_scene->addItem(edge);
        m_edgeItems.push_back(edge);
    }

    refreshEdges();
    // Auto-layout if no saved positions
    if(m_savedClassPos.isEmpty() && !m_classItems.isEmpty()){
        m_layout->applyAutoLayout(m_scene, m_classItems.values());
    }
}

void ProjectRelationView::refreshEdges(){
    for(auto *edge: m_edgeItems){
        QString fromSid = edge->data(1).toString();
        QString toSid = edge->data(2).toString();
        QPointF a = anchorForSid(fromSid);
        QPointF b = anchorForSid(toSid);
        static_cast<EdgeItem*>(edge)->setEndpoints(a,b);
    }
}

QString ProjectRelationView::layoutPath() const{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base+"/project_relation_layout.json";
}

void ProjectRelationView::saveLayout() const{
    QJsonArray classes;
    for(auto *ci: m_classItems){
        QJsonObject o; o["sid"]=ci->sid(); o["x"]=ci->pos().x(); o["y"]=ci->pos().y();
        // children
        QJsonArray kids;
        for(auto *ch : ci->childItems()){
            auto *mc = dynamic_cast<MethodChip*>(ch);
            if(!mc) continue;
            QJsonObject cj; cj["sid"]=mc->sid(); cj["lx"]=mc->pos().x(); cj["ly"]=mc->pos().y();
            kids.push_back(cj);
        }
        o["children"]=kids;
        classes.push_back(o);
    }
    QJsonObject root; root["classes"]=classes;
    QFile f(layoutPath());
    if(f.open(QIODevice::WriteOnly)){
        f.write(QJsonDocument(root).toJson());
        f.close();
    }
}

void ProjectRelationView::loadLayout(){
    m_savedClassPos.clear(); m_savedMethodLocalPos.clear();
    QFile f(layoutPath()); if(!f.open(QIODevice::ReadOnly)) return;
    auto obj = QJsonDocument::fromJson(f.readAll()).object(); f.close();
    for(auto v : obj.value("classes").toArray()){
        auto o=v.toObject(); QString sid = o.value("sid").toString();
        QPointF p(o.value("x").toDouble(), o.value("y").toDouble());
        m_savedClassPos.insert(sid, p);
        QHash<QString, QPointF> kids;
        for(auto kv : o.value("children").toArray()){
            auto ko = kv.toObject(); kids.insert(ko.value("sid").toString(), QPointF(ko.value("lx").toDouble(), ko.value("ly").toDouble()));
        }
        if(!kids.isEmpty()) m_savedMethodLocalPos.insert(sid, kids);
    }
}

void ProjectRelationView::contextMenuEvent(QContextMenuEvent *e){
    QPointF scenePos = mapToScene(e->pos());
    QGraphicsItem* item = scene()->itemAt(scenePos, transform());
    showContextMenuForItem(item, e->globalPos());
}

void ProjectRelationView::showContextMenuForItem(QGraphicsItem* item, const QPoint &globalPos){
    QMenu menu;
    if(auto *mc = dynamic_cast<MethodChip*>(item)){
        QAction *open = menu.addAction("Открыть в редакторе");
        QAction *rename = menu.addAction("Переименовать метод...");
        QAction *restore = nullptr;
        if(!mc->data().loc.file.isEmpty()) restore = menu.addAction("Откатить файл из stash");

        QAction *sel = menu.exec(globalPos);
        if(sel==open){
            emit openInEditor(mc->data().loc.file, mc->data().loc.line);
        } else if(sel==rename){
            bool ok=false; QString newName = QInputDialog::getText(this, "Переименование", "Новое имя метода:", QLineEdit::Normal, mc->data().canonicalName, &ok);
            if(ok && !newName.isEmpty()){
                if(m_patcher->renameMethod(mc->data(), newName)) rebuildGraph();
                else QMessageBox::warning(this, "Rename", "Не удалось переименовать метод.");
            }
        } else if(restore && sel==restore){
            if(m_patcher->restoreFromStash(mc->data().loc.file)) rebuildGraph();
        }
    } else if(auto *cc = dynamic_cast<ClassItem*>(item)){
        QAction *addM = menu.addAction("Добавить заглушку метода...");
        QAction *restore = nullptr;
        if(!cc->data().loc.file.isEmpty()) restore = menu.addAction("Откатить файл класса из stash");
        QAction *sel = menu.exec(globalPos);
        if(sel==addM){
            bool ok=false;
            QString ret = QInputDialog::getText(this, "Новый метод", "Тип возврата:", QLineEdit::Normal, "void", &ok); if(!ok) return;
            QString name = QInputDialog::getText(this, "Новый метод", "Имя метода:", QLineEdit::Normal, "newMethod", &ok); if(!ok) return;
            QVector<ParamInfo> params; // For MVP, no params dialog
            if(m_patcher->addMethodStub(cc->data(), ret, name, params)) rebuildGraph();
            else QMessageBox::warning(this, "Add method", "Не удалось добавить метод.");
        } else if(restore && sel==restore){
            if(m_patcher->restoreFromStash(cc->data().loc.file)) rebuildGraph();
        }
    }
}
