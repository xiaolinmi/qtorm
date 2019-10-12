#include "qormpropertymapping.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

QDebug operator<<(QDebug dbg, const QOrmPropertyMapping& propertyMapping)
{
    QDebugStateSaver saver{dbg};

    dbg << "QOrmPropertyMapping(" << propertyMapping.classPropertyName() << " => "
        << propertyMapping.tableFieldName() << ", " << propertyMapping.dataType();

    if (propertyMapping.isAutogenerated())
        dbg << ", autogenerated";

    if (propertyMapping.isObjectId())
        dbg << ", object ID";

    if (propertyMapping.isTransient())
        dbg << ", transient";

    dbg << ")";

    return dbg;
}

class QOrmPropertyMappingPrivate : public QSharedData
{
    friend class QOrmPropertyMapping;

    QOrmPropertyMappingPrivate(const QOrmMetadata& enclosingEntity,
                               QMetaProperty qMetaProperty,
                               QString classPropertyName,
                               QString tableFieldName,
                               bool isObjectId,
                               bool isAutoGenerated,
                               QVariant::Type dataType,
                               const QOrmMetadata* referencedEntity,
                               bool isTransient)
        : m_enclosingEntity{enclosingEntity}
        , m_qMetaProperty{std::move(qMetaProperty)}
        , m_classPropertyName{std::move(classPropertyName)}
        , m_tableFieldName{std::move(tableFieldName)}
        , m_isObjectId{isObjectId}
        , m_isAutogenerated{isAutoGenerated}
        , m_dataType{dataType}
        , m_referencedEntity{referencedEntity}
        , m_isTransient{isTransient}
    {
    }

    const QOrmMetadata& m_enclosingEntity;
    QMetaProperty m_qMetaProperty;
    QString m_classPropertyName;
    QString m_tableFieldName;
    bool m_isObjectId{false};
    bool m_isAutogenerated{false};
    QVariant::Type m_dataType{QVariant::Invalid};
    const QOrmMetadata* m_referencedEntity{nullptr};
    bool m_isTransient{false};
};

QOrmPropertyMapping::QOrmPropertyMapping(const QOrmMetadata& enclosingEntity,
                                         QMetaProperty qMetaProperty,
                                         QString classPropertyName,
                                         QString tableFieldName,
                                         bool isObjectId,
                                         bool isAutoGenerated,
                                         QVariant::Type dataType,
                                         const QOrmMetadata* referencedEntity,
                                         bool isTransient)
    : d{new QOrmPropertyMappingPrivate{enclosingEntity,
                                       std::move(qMetaProperty),
                                       std::move(classPropertyName),
                                       std::move(tableFieldName),
                                       isObjectId,
                                       isAutoGenerated,
                                       dataType,
                                       referencedEntity,
                                       isTransient}}
{
}

QOrmPropertyMapping::QOrmPropertyMapping(const QOrmPropertyMapping&) = default;

QOrmPropertyMapping::QOrmPropertyMapping(QOrmPropertyMapping&&) = default;

QOrmPropertyMapping::~QOrmPropertyMapping() = default;

QOrmPropertyMapping& QOrmPropertyMapping::operator=(const QOrmPropertyMapping&) = default;

QOrmPropertyMapping& QOrmPropertyMapping::operator=(QOrmPropertyMapping&&) = default;

const QOrmMetadata& QOrmPropertyMapping::enclosingEntity() const
{
    return d->m_enclosingEntity;
}

const QMetaProperty& QOrmPropertyMapping::qMetaProperty() const
{
    return d->m_qMetaProperty;
}

QString QOrmPropertyMapping::classPropertyName() const
{
    return d->m_classPropertyName;
}

QString QOrmPropertyMapping::tableFieldName() const
{
    return d->m_tableFieldName;
}

bool QOrmPropertyMapping::isObjectId() const
{
    return d->m_isObjectId;
}

bool QOrmPropertyMapping::isAutogenerated() const
{
    return d->m_isAutogenerated;
}

QVariant::Type QOrmPropertyMapping::dataType() const
{
    return d->m_dataType;
}

bool QOrmPropertyMapping::isReference() const
{
    return d->m_referencedEntity != nullptr;
}

const QOrmMetadata* QOrmPropertyMapping::referencedEntity() const
{
    return d->m_referencedEntity;
}

bool QOrmPropertyMapping::isTransient() const
{
    return d->m_isTransient;
}

QT_END_NAMESPACE
