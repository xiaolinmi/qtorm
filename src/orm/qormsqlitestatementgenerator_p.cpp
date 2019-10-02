#include "qormsqlitestatementgenerator_p.h"
#include "qormfilter.h"
#include "qormglobal_p.h"
#include "qormmetadata.h"
#include "qormmetadatacache.h"
#include "qormquery.h"
#include "qormrelation.h"

QT_BEGIN_NAMESPACE

static QString insertParameter(QVariantMap& boundParameters, QString name, QVariant value)
{
    QString key = ':' % name;

    for (int i = 0; boundParameters.contains(key); ++i)
        key = ':' % name % QString::number(i);

    boundParameters.insert(key, value);

    return key;    
}

std::pair<QString, QVariantMap> QOrmSqliteStatementGenerator::generate(const QOrmQuery& query)
{
    QVariantMap boundParameters;
    QString statement = generate(query, boundParameters);

    return std::make_pair(statement, boundParameters);
}

QString QOrmSqliteStatementGenerator::generate(const QOrmQuery& query, QVariantMap& boundParameters)
{
    switch (query.operation())
    {
        case QOrm::Operation::Create:
            return generateInsertStatement(*query.relation().mapping(),
                                           query.entityInstance(),
                                           boundParameters);

        case QOrm::Operation::Update:
            return generateUpdateStatement(*query.relation().mapping(),
                                           query.entityInstance(),
                                           boundParameters);

        case QOrm::Operation::Read:
            return generateSelectStatement(query, boundParameters);

        case QOrm::Operation::Delete:
            Q_ASSERT(query.relation().type() == QOrm::RelationType::Mapping);
            Q_ASSERT(query.filter().has_value());

            return generateDeleteStatement(*query.relation().mapping(),
                                           *query.filter(),
                                           boundParameters);

        default:
            qFatal("QtORM: %s: not implemented", __PRETTY_FUNCTION__);
    }
}

QString QOrmSqliteStatementGenerator::generateInsertStatement(const QOrmMetadata& relation,
                                                              const QObject* entityInstance,
                                                              QVariantMap& boundParameters)
{
    QStringList fieldsList;
    QStringList valuesList;

    for (const QOrmPropertyMapping& propertyMapping : relation.propertyMappings())
    {
        if (propertyMapping.isAutogenerated())
            continue;

        QString valueStr =
            insertParameter(boundParameters,
                            propertyMapping.tableFieldName(),
                            QOrmPrivate::propertyValue(entityInstance,
                                                       propertyMapping.classPropertyName()));
        valuesList.push_back(valueStr);
        fieldsList.push_back(propertyMapping.tableFieldName());
    }

    QString fieldsStr = fieldsList.join(',');
    QString valuesStr = valuesList.join(',');

    QString statement = QStringLiteral("INSERT INTO %1(%2) VALUES(%3)")
                            .arg(relation.tableName(), fieldsStr, valuesStr);

    return statement;
}

QString QOrmSqliteStatementGenerator::generateUpdateStatement(const QOrmMetadata& /*relation*/,
                                                              const QObject* /*instance*/,
                                                              QVariantMap& /*boundParameters*/)
{
    Q_ORM_NOT_IMPLEMENTED;
}

QString QOrmSqliteStatementGenerator::generateSelectStatement(const QOrmQuery& query,
                                                              QVariantMap& boundParameters)
{
    Q_ASSERT(query.operation() == QOrm::Operation::Read);

    QStringList parts = {"SELECT *", generateFromClause(query.relation(), boundParameters)};

    if (query.filter().has_value())
        parts += generateWhereClause(*query.filter(), boundParameters);

    return parts.join(QChar{' '});
}

QString QOrmSqliteStatementGenerator::generateDeleteStatement(const QOrmMetadata& relation,
                                                              const QOrmFilter& filter,
                                                              QVariantMap& boundParameters)
{
    QStringList parts = {"DELETE",
                         generateFromClause(QOrmRelation{relation}, boundParameters),
                         generateWhereClause(filter, boundParameters)};

    return parts.join(QChar{' '});
}

QString QOrmSqliteStatementGenerator::generateFromClause(const QOrmRelation& relation,
                                                         QVariantMap& boundParameters)
{
    switch (relation.type())
    {
        case QOrm::RelationType::Mapping:
            return QString{"FROM %1"}.arg(relation.mapping()->tableName());

        case QOrm::RelationType::Query:
            Q_ASSERT(relation.query()->operation() == QOrm::Operation::Read);

            return QString{"FROM (%1)"}.arg(generate(*relation.query(), boundParameters));
    }

    Q_ORM_UNEXPECTED_STATE;
}

QString QOrmSqliteStatementGenerator::generateWhereClause(const QOrmFilter& filter,
                                                          QVariantMap& boundParameters)
{
    QString whereClause;

    if (filter.type() == QOrm::FilterType::Expression)
    {
        Q_ASSERT(filter.expression() != nullptr);

        whereClause = generateCondition(*filter.expression(), boundParameters);

        if (!whereClause.isEmpty())
            whereClause = "WHERE " + whereClause;
    }

    return whereClause;
}

QString QOrmSqliteStatementGenerator::generateCondition(const QOrmFilterExpression& expression,
                                                        QVariantMap& boundParameters)
{
    switch (expression.type())
    {
        case QOrm::FilterExpressionType::TerminalPredicate:
            Q_ASSERT(expression.terminalPredicate() != nullptr);
            return generateCondition(*expression.terminalPredicate(), boundParameters);

        case QOrm::FilterExpressionType::BinaryPredicate:
            Q_ASSERT(expression.binaryPredicate() != nullptr);
            return generateCondition(*expression.binaryPredicate(), boundParameters);

        case QOrm::FilterExpressionType::UnaryPredicate:
            Q_ASSERT(expression.unaryPredicate() != nullptr);
            return generateCondition(*expression.unaryPredicate(), boundParameters);
    }

    Q_ORM_UNEXPECTED_STATE;
}

QString
QOrmSqliteStatementGenerator::generateCondition(const QOrmFilterTerminalPredicate& predicate,
                                                QVariantMap& boundParameters)
{
    Q_ASSERT(predicate.isResolved());

    static const QHash<QOrm::Comparison, QString> comparisonOps = {
        {QOrm::Comparison::Less, "<"},
        {QOrm::Comparison::Equal, "="},
        {QOrm::Comparison::Greater, ">"},
        {QOrm::Comparison::NotEqual, "<>"},
        {QOrm::Comparison::LessOrEqual, "<="},
        {QOrm::Comparison::GreaterOrEqual, ">="}};

    Q_ASSERT(comparisonOps.contains(predicate.comparison()));

    QString parameterKey = insertParameter(
        boundParameters, predicate.propertyMapping()->tableFieldName(), predicate.value());

    QString statement = QString{"%1 %2 %3"}.arg(predicate.propertyMapping()->tableFieldName(),
                                                comparisonOps[predicate.comparison()],
                                                parameterKey);

    return statement;
}

QString QOrmSqliteStatementGenerator::generateCondition(const QOrmFilterBinaryPredicate& predicate,
                                                        QVariantMap& boundParameters)
{
    QString lhsExpr = generateCondition(predicate.lhs(), boundParameters);
    QString rhsExpr = generateCondition(predicate.rhs(), boundParameters);

    QString op;

    switch (predicate.logicalOperator())
    {
        case QOrm::BinaryLogicalOperator::Or:
            op = "OR";
            break;
        case QOrm::BinaryLogicalOperator::And:
            op = "AND";
            break;
    }

    Q_ASSERT(!op.isEmpty());

    return QString{"(%1) %2 (%3)"}.arg(lhsExpr, op, rhsExpr);
}

QString QOrmSqliteStatementGenerator::generateCondition(const QOrmFilterUnaryPredicate& predicate,
                                                        QVariantMap& boundParameters)
{
    QString rhsExpr = generateCondition(predicate.rhs(), boundParameters);
    Q_ASSERT(predicate.logicalOperator() == QOrm::UnaryLogicalOperator::Not);

    return QString{"NOT (%1)"}.arg(rhsExpr);
}

QT_END_NAMESPACE
