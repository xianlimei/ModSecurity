/*
 * ModSecurity, http://www.modsecurity.org/
 * Copyright (c) 2015 Trustwave Holdings, Inc. (http://www.trustwave.com/)
 *
 * You may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * If any of the files related to licensing are missing or if you have any
 * other questions related to licensing please contact Trustwave Holdings, Inc.
 * directly using the email address security@modsecurity.org.
 *
 */

#ifdef __cplusplus
#include <stack>
#include <vector>
#include <string>
#include <list>
#include <memory>
#include <utility>
#endif

#ifndef HEADERS_MODSECURITY_RULE_WITH_ACTIONS_H_
#define HEADERS_MODSECURITY_RULE_WITH_ACTIONS_H_

#include "modsecurity/transaction.h"
#include "modsecurity/modsecurity.h"
#include "modsecurity/variable_value.h"
#include "modsecurity/rule.h"

#ifdef __cplusplus

namespace modsecurity {


class RuleWithActions : public Rule {
 public:
    RuleWithActions(
        Actions *a,
        Transformations *t,
        std::unique_ptr<std::string> fileName,
        int lineNumber);

    ~RuleWithActions();

    bool evaluate(Transaction *transaction, std::shared_ptr<RuleMessage> ruleMessage) override;

    virtual bool evaluate(Transaction *transaction) override;


    void executeActionsIndependentOfChainedRuleResult(
        Transaction *trasn,
        bool *containsDisruptive,
        std::shared_ptr<RuleMessage> ruleMessage);

    void executeActionsAfterFullMatch(
        Transaction *trasn,
        bool containsDisruptive,
        std::shared_ptr<RuleMessage> ruleMessage);

    void executeAction(Transaction *trans,
        bool containsBlock,
        std::shared_ptr<RuleMessage> ruleMessage,
        actions::Action *a,
        bool context);


    void executeTransformations(
        Transaction *trasn, const std::string &value, TransformationResults &ret);

    inline void executeTransformation(actions::transformations::Transformation *a,
        std::shared_ptr<std::string> *value,
        Transaction *trans,
        TransformationResults *ret,
        std::string *path,
        int *nth);


    void performLogging(Transaction *trans,
        std::shared_ptr<RuleMessage> ruleMessage,
        bool lastLog = true,
        bool chainedParentNull = false);

    std::vector<actions::Action *> getActionsByName(const std::string& name,
        Transaction *t);
    bool containsTag(const std::string& name, Transaction *t);
    bool containsMsg(const std::string& name, Transaction *t);

    inline bool isChained() { return m_isChained == true; }
    inline bool hasCaptureAction() { return m_containsCaptureAction == true; }
    inline void setChained(bool b) { m_isChained = b; }
    inline bool hasDisruptiveAction() { return m_disruptiveAction != NULL; }
    inline bool hasBlockAction() { return m_containsStaticBlockAction == true; }
    inline bool hasMultimatch() { return m_containsMultiMatchAction == true; }
    inline bool hasBlock() { return m_containsStaticBlockAction == true; }

    inline bool hasLogData() { return m_logData != NULL; }
    std::string logData(Transaction *t);
    inline bool hasMsg() { return m_msg != NULL; }
    std::string msg(Transaction *t);
    inline bool hasSeverity() { return m_severity != NULL; }
    int severity();

    std::string m_marker;
    std::string m_rev;
    std::string m_ver;
    int m_accuracy;
    int m_maturity;


    int64_t m_ruleId;

    std::unique_ptr<RuleWithActions> m_chainedRuleChild;
    RuleWithActions *m_chainedRuleParent;

 private:
    /* actions */
    actions::Action *m_disruptiveAction;
    actions::LogData *m_logData;
    actions::Msg *m_msg;
    actions::Severity *m_severity;
    MatchActions m_actionsRuntimePos;
    SetVars m_actionsSetVar;
    Tags m_actionsTag;

    /* actions > transformations */
    Transformations m_transformations;

    bool m_containsCaptureAction:1;
    bool m_containsMultiMatchAction:1;
    bool m_containsStaticBlockAction:1;
    bool m_isChained:1;
};

}  // namespace modsecurity
#endif


#endif  // HEADERS_MODSECURITY_RULE_WITH_ACTIONS_H_