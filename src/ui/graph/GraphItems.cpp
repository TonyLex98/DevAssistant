#include "ui/graph/GraphItems.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <cmath>

// ---- ClassItem ----
ClassItem::ClassItem(const NodeData &data): m_data(data){
    setFlags(ItemIsMovable|ItemIsSelectable|ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setAcceptHoverEvents(true);
    setZValue(0);
    setCursor(QCursor(Qt::OpenHandCursor));
    m_header = data.displayName;
}

void ClassItem::ensureInitialLayout(){
    if(m_layoutInitialized) return;
    // Simple vertical stack initial layout
    qreal w = 320; qreal y = 42;
    for(auto *ch: m_children){
        ch->setPos(12, y);
        y += ch->boundingRect().height() + 8;
        w = std::max(w, ch->boundingRect().width()+24);
    }
    m_rectCache = QRectF(0,0,w+24, std::max<qreal>(y+12, 90));
    m_layoutInitialized = true;
}

QRectF ClassItem::boundingRect() const{ return m_rectCache; }

void ClassItem::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*){
    ensureInitialLayout();
    p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(220,40,40)); pen.setWidthF(isSelected()?3.0:2.0); p->setPen(pen);
    QBrush br(QColor(255,220,220)); p->setBrush(br);
    const qreal radius = 18.0; p->drawRoundedRect(m_rectCache, radius, radius);

    QFont f = p->font(); f.setBold(true); f.setPointSizeF(f.pointSizeF()+1); p->setFont(f);
    p->drawText(QRectF(12,10,m_rectCache.width()-24,24), Qt::AlignLeft|Qt::AlignVCenter, m_header);
}

void ClassItem::addChildChip(MethodChip* chip){
    m_children.append(chip);
    chip->setParentItem(this);
    m_layoutInitialized=false; // re-layout once on next paint
    update();
}

QPointF ClassItem::chipAnchor(const MethodChip* chip) const{
    return mapToScene(chip->pos()+chip->boundingRect().center());
}

QJsonObject ClassItem::toJson() const{
    QJsonObject o; o["sid"]=m_data.sid.id; o["x"]=pos().x(); o["y"]=pos().y();
    // children local positions
    QJsonArray kids;
    for(auto *ch: m_children){
        QJsonObject cj = ch->toJson();
        kids.push_back(cj);
    }
    o["children"]=kids;
    return o;
}

void ClassItem::fromJson(const QJsonObject &o){
    setPos(o.value("x").toDouble(), o.value("y").toDouble());
    auto kids = o.value("children").toArray();
    Q_UNUSED(kids); // applied later when children exist
}

QVariant ClassItem::itemChange(GraphicsItemChange change, const QVariant &value){
    if(change==ItemPositionHasChanged) emit positionChanged();
    return QGraphicsObject::itemChange(change,value);
}

QRectF MethodChip::boundingRect() const{ return m_rect; }

void MethodChip::paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*){
    p->setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(120,120,120)); pen.setWidthF(isSelected()?2.0:1.5); p->setPen(pen);
    QBrush br(QColor(250,250,250)); p->setBrush(br);
    p->drawRoundedRect(m_rect, 10, 10);
    QFont f=p->font(); p->setFont(f);
    p->drawText(m_rect.adjusted(10,6,-10,-6), Qt::AlignLeft|Qt::AlignVCenter, m_data.displayName);
}

void MethodChip::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e){
    emit openRequested(m_data.loc.file, m_data.loc.line);
    QGraphicsObject::mouseDoubleClickEvent(e);
}

QVariant MethodChip::itemChange(GraphicsItemChange change, const QVariant &value){
    if(change==ItemPositionHasChanged) emit positionChanged();
    return QGraphicsObject::itemChange(change,value);
}

QJsonObject MethodChip::toJson() const{
    QJsonObject o; o["sid"]=m_data.sid.id; o["lx"]=pos().x(); o["ly"]=pos().y();
    return o;
}

void MethodChip::fromJson(const QJsonObject &o){
    setPos(o.value("lx").toDouble(), o.value("ly").toDouble());
}

// ---- EdgeItem ----
EdgeItem::EdgeItem(const QString &fromSid, const QString &toSid, EdgeKind kind): m_from(fromSid), m_to(toSid), m_kind(kind){
    setZValue(-1); m_font.setPointSizeF(9.0);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

void EdgeItem::setEndpoints(const QPointF &a, const QPointF &b){
    m_a=a; m_b=b;
    QPainterPath path(m_a);
    QPointF ctrl = (m_a + m_b)/2.0 + QPointF(0, -40);
    path.quadTo(ctrl, m_b);
    setPath(path);
    update();
}

void EdgeItem::paint(QPainter *p, const QStyleOptionGraphicsItem *o, QWidget *w){
    Q_UNUSED(o); Q_UNUSED(w);
    p->setRenderHint(QPainter::Antialiasing, true);

    QPen pen;
    pen.setWidthF(1.8);

    // base color by edge kind
    switch(m_kind){
        case EdgeKind::Calls:        pen.setColor(QColor(40,120,220)); break;
        case EdgeKind::Instantiates: pen.setColor(QColor(40,180,120)); pen.setStyle(Qt::DashLine); break;
        case EdgeKind::Accesses:     pen.setColor(QColor(160,160,50));  pen.setStyle(Qt::DotLine);  break;
        case EdgeKind::Inherits:     pen.setColor(QColor(120,60,180));  break;
        default:                     pen.setColor(Qt::black);           break;
    }

    if (m_lost){
        pen.setColor(QColor(220,160,20));
        pen.setStyle(Qt::DashDotDotLine);
        pen.setWidthF(2.4);
    }

    if (m_highlight){
        pen.setColor(QColor(30,144,255)); // dodger blue
        pen.setStyle(Qt::SolidLine);
        pen.setWidthF(3.0);
    }

    p->setPen(pen);
    p->setBrush(Qt::NoBrush);

    // draw the path ourselves instead of calling the base paint()
    p->drawPath(this->path());

    // arrowhead
    const qreal arrowSize = 8.0;
    QPainterPath path = this->path();
    QPointF end = path.pointAtPercent(1.0);
    QPointF prev = path.pointAtPercent(0.98);
    QPointF dir = end - prev;
    qreal len = std::hypot(dir.x(), dir.y());
    if (len > 1e-3) dir /= len;
    QPointF ort(-dir.y(), dir.x());
    QPointF p1 = end - dir*arrowSize + ort*(arrowSize*0.6);
    QPointF p2 = end - dir*arrowSize - ort*(arrowSize*0.6);
    QPolygonF arrow; arrow << end << p1 << p2;
    p->setBrush(pen.color());
    p->drawPolygon(arrow);

    // label/order are not drawn yet
}


void ClassItem::mousePressEvent(QGraphicsSceneMouseEvent *e){
    setCursor(QCursor(Qt::ClosedHandCursor));
    emit heldChanged(true);                         // grabbed
    QGraphicsObject::mousePressEvent(e);
}

void ClassItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *e){
    setCursor(QCursor(Qt::OpenHandCursor));
    emit heldChanged(false);                        // released
    QGraphicsObject::mouseReleaseEvent(e);
}

// ---- MethodChip ----
MethodChip::MethodChip(const NodeData &data, QGraphicsItem* parent)
    : QGraphicsObject(parent), m_data(data){
    setFlags(ItemIsMovable|ItemIsSelectable|ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setAcceptHoverEvents(true);
    setZValue(1);
    setCursor(QCursor(Qt::OpenHandCursor));
}

// paint / toJson / fromJson are defined above

void MethodChip::mousePressEvent(QGraphicsSceneMouseEvent *e){
    setCursor(QCursor(Qt::ClosedHandCursor));
    emit heldChanged(true);
    QGraphicsObject::mousePressEvent(e);
}

void MethodChip::mouseReleaseEvent(QGraphicsSceneMouseEvent *e){
    setCursor(QCursor(Qt::OpenHandCursor));
    emit heldChanged(false);
    QGraphicsObject::mouseReleaseEvent(e);
}
