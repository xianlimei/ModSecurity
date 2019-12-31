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
#include <cstring>
#endif

#ifndef HEADERS_MODSECURITY_RULE_MESSAGE_H_
#define HEADERS_MODSECURITY_RULE_MESSAGE_H_

#include "modsecurity/transaction.h"
#include "modsecurity/rule.h"


#ifdef __cplusplus

namespace modsecurity {
namespace actions {
class Tag;
};
class RuleWithActions;

class RuleMessage {
 public:
    enum LogMessageInfo {
        ErrorLogTailLogMessageInfo = 2,
        ClientLogMessageInfo = 4
    };

    RuleMessage(const RuleMessage *rule) :
        m_data(rule->m_data),
        m_isDisruptive(rule->m_isDisruptive),
        m_match(rule->m_match),
        m_message(rule->m_message),
        m_reference(rule->m_reference),
        m_rule(rule->m_rule),
        m_saveMessage(rule->m_saveMessage),
        m_transaction(rule->m_transaction)
    { }


    RuleMessage(Transaction *transaction) :
        m_transaction(transaction),
        m_rule(nullptr),
        m_data(""),
        m_isDisruptive(false),
        m_match(""),
        m_message(""),
        m_reference(""),
        m_saveMessage(true),
        m_severity(0)
    { }


    void clean() {
        m_data = "";
        m_match = "";
        m_isDisruptive = false;
        m_reference = "";
        m_severity = 0;
    }

    std::string log() {
        return RuleMessage::log(this, 0);
    }
    std::string log(int props) {
        return RuleMessage::log(this, props);
    }
    std::string log(int props, int responseCode) {
        return RuleMessage::log(this, props, responseCode);
    }
    std::string errorLog() {
        return RuleMessage::log(this,
		ClientLogMessageInfo | ErrorLogTailLogMessageInfo);
    }

    static std::string log(RuleMessage *rm, int props, int code);
    static std::string log(RuleMessage *rm, int props) {
        return RuleMessage::log(rm, props, -1);
    }
    static std::string log(RuleMessage *rm) {
        return RuleMessage::log(rm, 0);
    }

    static std::string _details(RuleMessage *rm);
    static std::string _errorLogTail(RuleMessage *rm);

    const RuleWithActions *getRule() {
        return m_rule;
    }
    void setRule(const RuleWithActions *rule) {
        m_rule = rule;
    }

    bool isSettle() {
        return m_rule != nullptr;
    }

    int getRuleId();
    int getPhase();
    std::string getFileName();
    int getLineNumber();
    std::string getRev();
    std::string getVer();
    int getMaturity();
    int getAccuracy();
    std::vector<actions::Tag *> /* RuleWithActions::Tags */ getTags();

    std::string getClientIpAddress();
    std::string getServerIpAddress();
    std::string getRequestId();
    std::string getUri();

    void setItToNotBeSaved() {
        m_saveMessage = false;
    }

    void setItToBeSaved() {
        m_saveMessage = true;
    }

    bool isItToBeSaved() {
        return m_saveMessage;
    }


    // Rule
    bool m_isDisruptive;
    int m_severity;

    // Transaction
    std::string m_data;
    std::string m_match;

    std::string m_message;
    std::string m_reference;

 private:
     bool m_saveMessage:1;
    Transaction *m_transaction;
    const RuleWithActions *m_rule;
};


}  // namespace modsecurity
#endif


#endif  // HEADERS_MODSECURITY_RULE_MESSAGE_H_
