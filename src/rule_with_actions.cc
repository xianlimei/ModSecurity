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

#include "modsecurity/rule.h"

#include <stdio.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "modsecurity/actions/action.h"
#include "modsecurity/modsecurity.h"
#include "modsecurity/rule_message.h"
#include "modsecurity/rules_set.h"
#include "src/rule_with_actions.h"
#include "src/actions/block.h"
#include "src/actions/capture.h"
#include "src/actions/log_data.h"
#include "src/actions/msg.h"
#include "src/actions/multi_match.h"
#include "src/actions/set_var.h"
#include "src/actions/severity.h"
#include "src/actions/tag.h"
#include "src/actions/transformations/transformation.h"
#include "src/actions/xmlns.h"
#include "src/utils/string.h"


namespace modsecurity {

RuleWithActions::RuleWithActions(
    Actions *actions,
    Transformations *transformations,
    std::unique_ptr<std::string> fileName,
    int lineNumber)
    : Rule(std::move(fileName), lineNumber),
    m_accuracy(0),
    m_actionsRuntimePos(),
    m_transformations(transformations != NULL ? *transformations : Transformations()),
    m_actionsSetVar(),
    m_actionsTag(),
    m_isChained(false),
    m_maturity(0),
    m_revision(""),
    m_ruleId(0),
    m_version(""),
    m_disruptiveAction(nullptr),
    m_containsBlockAction(false),
    m_containsCaptureAction(false),
    m_containsMultiMatchAction(false),
    m_severity(nullptr),
    m_logData(nullptr),
    m_chainedRuleChild(nullptr),
    m_chainedRuleParent(nullptr),
    m_msg(nullptr) {
    if (actions) {
        for (Action *a : *actions) {
            a->ruleInit(this);
            if (a->getActionKind() == actions::ActionKind::ConfigurationKind) {
                a->execute(NULL);
                delete a;
            } else if (a->getActionKind() == actions::ActionKind::RunTimeOnlyIfMatchKind) {
                if (dynamic_cast<actions::Capture *>(a)) {
                    m_containsCaptureAction = true;
                    delete a;
                } else if (dynamic_cast<actions::MultiMatch *>(a)) {
                    m_containsMultiMatchAction = true;
                    delete a;
                } else if (dynamic_cast<actions::Severity *>(a)) {
                    m_severity = dynamic_cast<actions::Severity *>(a);
                } else if (dynamic_cast<actions::LogData *>(a)) {
                    m_logData = dynamic_cast<actions::LogData*>(a);
                } else if (dynamic_cast<actions::Msg *>(a)) {
                    m_msg = dynamic_cast<actions::Msg*>(a);
                } else if (dynamic_cast<actions::SetVar *>(a)) {
                    m_actionsSetVar.push_back(
                        dynamic_cast<actions::SetVar *>(a));
                } else if (dynamic_cast<actions::Tag *>(a)) {
                    m_actionsTag.push_back(dynamic_cast<actions::Tag *>(a));
                } else if (dynamic_cast<actions::Block *>(a)) {
                    m_actionsRuntimePos.push_back(a);
                    m_containsBlockAction = true;
                    RuleWithActions *parent = m_chainedRuleParent;
                    while (parent) {
                        parent->setBlockAction(true);
                        parent = parent->m_chainedRuleParent;
                    }
                } else if (dynamic_cast<actions::XmlNS *>(a)) {
                    m_XmlNSs.push_back(dynamic_cast<actions::XmlNS *>(a));
                } else if (a->isDisruptive() == true) {
                    if (m_disruptiveAction != nullptr) {
                        delete m_disruptiveAction;
                        m_disruptiveAction = nullptr;
                    }
                    m_disruptiveAction = a;
                } else {
                    m_actionsRuntimePos.push_back(a);
                }
            } else {
                delete a;
                std::cout << "General failure, action: " << *a->getName();
                std::cout << " has an unknown type." << std::endl;
                throw;
            }
        }
        delete actions;
    }
}


RuleWithActions::~RuleWithActions() {
    if (m_disruptiveAction != nullptr) {
        delete m_disruptiveAction;
        m_disruptiveAction = nullptr;
    }
    if (m_logData) {
        delete m_logData;
        m_logData = nullptr;
    }
    if (m_msg) {
        delete m_msg;
        m_msg = nullptr;
    }
    if (m_severity) {
        delete m_severity;
        m_severity = nullptr;
    }

    while (m_actionsRuntimePos.empty() == false) {
        auto *a = m_actionsRuntimePos.back();
        m_actionsRuntimePos.pop_back();
        delete a;
    }
    while (m_actionsSetVar.empty() == false) {
        auto *a = m_actionsSetVar.back();
        m_actionsSetVar.pop_back();
        delete a;
    }
    while (m_actionsTag.empty() == false) {
        auto *a = m_actionsTag.back();
        m_actionsTag.pop_back();
        delete a;
    }
    while (m_XmlNSs.empty() == false) {
        auto *a = m_XmlNSs.back();
        m_XmlNSs.pop_back();
        delete a;
    }
    while (m_transformations.empty() == false) {
        auto *a = m_transformations.back();
        m_transformations.pop_back();
        delete a;
    }
}


bool RuleWithActions::evaluate(Transaction *transaction) const {
    /* Matched vars needs to be clear at every new rule execution */
    transaction->m_matched.clear();

    return true;
}


void RuleWithActions::executeActionsIndependentOfChainedRuleResult(
    Transaction *trans) const {
    /**
     * FIXME: this is not conteplating SecRuleUpdateActionBy*.
     *
     */
    for (actions::SetVar *a : m_actionsSetVar) {
        ms_dbg_a(trans, 4, "Running [independent] (non-disruptive) " \
            "action: " + *a->getName());

        a->execute(trans);
    }

    if (m_severity) {
        m_severity->execute(trans);
    }

    if (m_logData) {
        m_logData->execute(trans);
    }

    if (m_msg) {
        m_msg->execute(trans);
    }
}


void RuleWithActions::executeActionsAfterFullMatch(Transaction *trans) const {
    bool disruptiveAlreadyExecuted = false;

    /* Default Actions */
    /**
     * FIXME (transaction.cc): there should be a:
     *       getDefaultActionlFor(phase, kind)
     *       in transaction.
     * 
     */
    for (auto &a : trans->m_rules->m_defaultActions[getPhase()]) {
        if (a.get()->getActionKind() != actions::ActionKind::RunTimeOnlyIfMatchKind) {
            continue;
        }
        if (!a.get()->isDisruptive()) {
            executeAsDefaulAction(trans, a.get(), true);
        }
    }

    for (actions::Tag *a : m_actionsTag) {
        ms_dbg_a(trans, 4, "Running (non-disruptive) action: " \
            + a->getTagName(trans));
        a->execute(trans);
    }

    /**
     * 
     * FIXME: SecRuleUpdateActionBy should be runtime
     * 
     */
    for (auto &b :
        trans->m_rules->m_exceptions.m_action_pos_update_target_by_id) {
        if (m_ruleId != b.first) {
            continue;
        }
        actions::Action *a = dynamic_cast<actions::Action*>(b.second.get());
        executeAction(trans, a, false);
        disruptiveAlreadyExecuted = true;
    }

    for (Action *a : m_actionsRuntimePos) {
        if (!a->isDisruptive()
                && !(disruptiveAlreadyExecuted
                && dynamic_cast<actions::Block *>(a))) {
            if (dynamic_cast<actions::Block *>(a)) {
                executeBlockAction(trans);
            } else {
                executeAction(trans, a, false);
            }
        }
    }

    if (!disruptiveAlreadyExecuted && m_disruptiveAction != nullptr) {
        executeAction(trans, m_disruptiveAction, false);
    }
}



void RuleWithActions::executeAsDefaulAction(Transaction *trans,
    Action *a, bool defaultContext) const {
    if (a->isDisruptive() == false) {
        ms_dbg_a(trans, 9, "Running action: " + *a->getName());
        a->executeAsDefaulAction(trans, this);
        return;
    }


    if (defaultContext && !hasBlockAction()) {
        ms_dbg_a(trans, 4, "Ignoring action: " + *a->getName() + \
            " (rule does not cotains block)");
        return;
    }

    if (trans->getRuleEngineState() == RulesSet::EnabledRuleEngine) {
        ms_dbg_a(trans, 4, "Running (disruptive)     action: " + 
            *a->getName() + ".");
        a->executeAsDefaulAction(trans, this);
        return;
    }

    ms_dbg_a(trans, 4, "Not running disruptive action: " \
        + *a->getName() + ". SecRuleEngine is not On.");
}


void RuleWithActions::executeAction(Transaction *trans,
    Action *a, bool defaultContext) const {
    if (a->isDisruptive() == false) {
        ms_dbg_a(trans, 9, "Running action: " + *a->getName());
        a->execute(trans);
        return;
    }

    if (defaultContext && !hasBlockAction()) {
        ms_dbg_a(trans, 4, "Ignoring action: " + *a->getName() + \
            " (rule does not cotains block)");
        return;
    }

    if (trans->getRuleEngineState() == RulesSet::EnabledRuleEngine) {
        ms_dbg_a(trans, 4, "Running (disruptive)     action: " + 
            *a->getName() + ".");
        a->execute(trans);
        return;
    }

    ms_dbg_a(trans, 4, "Not running disruptive action: " \
        + *a->getName() + ". SecRuleEngine is not On.");
}


void RuleWithActions::executeTransformations(
    Transaction *trans,
    const std::string &in,
    TransformationsResults &results) const {
    int none = 0;

    ModSecStackString ssin{results.m_stackAllocator};
    ssin.assign(in.c_str(), in.size());
    results.push_back(TransformationResult(ssin));


    std::string path("");
    std::shared_ptr<std::string> value =
        std::shared_ptr<std::string>(new std::string(in));

    for (Transformation *a : m_transformations) {
        if (a->isNone()) {
            none++;
        }
    }

    // Check for transformations on the SecDefaultAction
    // Notice that first we make sure that won't be a t:none
    // on the target rule.
    if (none == 0) {
        for (auto &a : trans->m_rules->m_defaultActions[getPhase()]) {
            if (a->getActionKind() \
                != actions::ActionKind::RunTimeBeforeMatchAttemptKind) {
                continue;
            }

            // FIXME: here the object needs to be a transformation already.
            Transformation *t = dynamic_cast<Transformation *>(a.get());
            executeTransformation(trans, &results, t);
        }
    }

    for (Transformation *a : m_transformations) {
        if (none == 0) {
            executeTransformation(trans, &results, a);
        }
        if (a->isNone()) {
            none--;
        }
    }

    // FIXME: It can't be something different from transformation. Sort this
    //        on rules compile time.
    for (auto &b :
        trans->m_rules->m_exceptions.m_action_pre_update_target_by_id) {
        if (m_ruleId != b.first) {
            continue;
        }
        Transformation *a = dynamic_cast<Transformation*>(b.second.get());
        if (a->isNone()) {
            none++;
        }
    }

    for (auto &b :
        trans->m_rules->m_exceptions.m_action_pre_update_target_by_id) {
        if (m_ruleId != b.first) {
            continue;
        }
        Transformation *a = dynamic_cast<Transformation*>(b.second.get());
        if (none == 0) {
            executeTransformation(trans, &results, a);
        }
        if (a->isNone()) {
            none--;
        }
    }

    if (m_containsMultiMatchAction == true) {
        ms_dbg_a(trans, 9, "multiMatch is enabled. " \
            + std::to_string(results.size()) + \
            " values to be tested.");
    } else {
        //results.push_back(TransformationResult(nullptr, ssin));
        //results.pop_front();
    }
}


inline void RuleWithActions::executeTransformation(
    Transaction *transaction,
    TransformationsResults *ret,
    Transformation *transformation) const {
    executeTransformation(
        transaction,
        *ret->back().getAfter(),
        ret,
        transformation
    );
}


inline void RuleWithActions::executeTransformation(
    Transaction *transaction,
    ModSecStackString &in,
    TransformationsResults *ret,
    Transformation *transformation) const {

    ModSecStackString out{ret->m_stackAllocator};
    transformation->execute(transaction, in, out);

    ms_dbg_a(transaction, 9, " T (" + std::to_string(ret->size() - 1) + ") " + \
        *transformation->getName() + ": \"" + \
        utils::string::limitTo(80, out.c_str()) + "\"");

    ret->push_back(
        TransformationResult(
            out,
            transformation->getName()
        )
    );
}


bool RuleWithActions::containsTag(const std::string& name, Transaction *t) const {
    for (auto &tag : m_actionsTag) {
        if (tag != NULL && tag->getTagName(t) == name) {
            return true;
        }
    }
    return false;
}


bool RuleWithActions::containsMsg(const std::string& name, Transaction *t) const {
    return m_msg && m_msg->data(t) == name;
}


void RuleWithActions::performLogging(Transaction *trans,
    bool lastLog,
    bool chainedParentNull) const {
    RuleMessage *rm = trans->messageGetLast();

    /* last rule in the chain. */
    bool isItToBeLogged = rm->isItToBeSaved();

    /**
    *
    * RuleMessage is stacked allocated for the rule execution,
    * anything beyond this may lead to invalid pointer access.
    *
    * In case of a warning, o set of messages is saved to be read
    * at audit log generation. Therefore demands a copy here.
    *
    * FIXME: Study an way to avoid the copy.
    *
    **/
    if (lastLog) {
        if (chainedParentNull) {
            isItToBeLogged = (isItToBeLogged && (m_chainedRuleParent == nullptr));
            if (isItToBeLogged && !hasMultimatch()) {
                /* warn */
                trans->messageLog(this);
            }

            /* error */
            if (!rm->m_isDisruptive) {
                trans->serverLog(rm);
            }
        } else {
            if (isItToBeLogged && !hasMultimatch()
                && !rm->m_message.empty()) {
                /* warn */
                trans->messageLog(this);

                /* error */
                if (!rm->m_isDisruptive) {
                    trans->serverLog(rm);
                }
            }
            else if (hasBlockAction() && !hasMultimatch()) {
                /* warn */
                trans->messageLog(this);

                /* error */
                if (!rm->m_isDisruptive) {
                    trans->serverLog(rm);
                }
            }
        }
    } else {
        if (hasMultimatch() && isItToBeLogged) {
            /* warn */
            trans->messageLog(this);

            /* error */
            if (!rm->m_isDisruptive) {
                trans->serverLog(rm);
            }

            //ruleMessage.clean();
        }
    }
}

std::string RuleWithActions::getLogData(Transaction *t) const { return m_logData->data(t); }
std::string RuleWithActions::getMsg(Transaction *t) const { return m_msg->data(t); }
int RuleWithActions::getSeverity() const { return m_severity->m_severity; }


}  // namespace modsecurity
