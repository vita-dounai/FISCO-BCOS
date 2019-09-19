/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/** @file TablePrecompiled.h
 *  @author ancelmo
 *  @date 20180921
 */
#include "TablePrecompiled.h"
#include "ConditionPrecompiled.h"
#include "EntriesPrecompiled.h"
#include "EntryPrecompiled.h"
#include "StorageException.h"
#include "Table.h"
#include <libblockverifier/ExecutiveContext.h>
#include <libdevcore/easylog.h>
#include <libethcore/ABI.h>

using namespace dev;
using namespace dev::blockverifier;
using namespace dev::storage;

const char* const TABLE_METHOD_SLT_STR_ADD = "select(string,address)";
const char* const TABLE_METHOD_INS_STR_ADD = "insert(string,address)";
const char* const TABLE_METHOD_NEWCOND = "newCondition()";
const char* const TABLE_METHOD_NEWENT = "newEntry()";
const char* const TABLE_METHOD_RE_STR_ADD = "remove(string,address)";
const char* const TABLE_METHOD_UP_STR_2ADD = "update(string,address,address)";


TablePrecompiled::TablePrecompiled()
{
    name2Selector[TABLE_METHOD_SLT_STR_ADD] = getFuncSelector(TABLE_METHOD_SLT_STR_ADD);
    name2Selector[TABLE_METHOD_INS_STR_ADD] = getFuncSelector(TABLE_METHOD_INS_STR_ADD);
    name2Selector[TABLE_METHOD_NEWCOND] = getFuncSelector(TABLE_METHOD_NEWCOND);
    name2Selector[TABLE_METHOD_NEWENT] = getFuncSelector(TABLE_METHOD_NEWENT);
    name2Selector[TABLE_METHOD_RE_STR_ADD] = getFuncSelector(TABLE_METHOD_RE_STR_ADD);
    name2Selector[TABLE_METHOD_UP_STR_2ADD] = getFuncSelector(TABLE_METHOD_UP_STR_2ADD);
}

std::string TablePrecompiled::toString()
{
    return "Table";
}

void TablePrecompiled::checkLengthValidate(
    const std::string& field_value, int32_t max_length, int32_t throw_exception)
{
    if (field_value.size() > (size_t)max_length)
    {
        STORAGE_LOG(ERROR) << "key:" << field_value << " value size:" << field_value.size()
                           << " greater than " << max_length;
        BOOST_THROW_EXCEPTION(StorageException(throw_exception,
            std::string("size of value of key greater than") + std::to_string(max_length)));
    }
}

bytes TablePrecompiled::call(
    ExecutiveContext::Ptr context, bytesConstRef param, Address const& origin)
{
    STORAGE_LOG(TRACE) << LOG_BADGE("TablePrecompiled") << LOG_DESC("call")
                       << LOG_KV("param", toHex(param));

    uint32_t func = getParamFunc(param);
    bytesConstRef data = getParamData(param);

    dev::eth::ContractABI abi;

    bytes out;

    if (func == name2Selector[TABLE_METHOD_SLT_STR_ADD])
    {  // select(string,address)
        std::string key;
        Address conditionAddress;
        abi.abiOut(data, key, conditionAddress);

        ConditionPrecompiled::Ptr conditionPrecompiled =
            std::dynamic_pointer_cast<ConditionPrecompiled>(
                context->getPrecompiled(conditionAddress));
        auto condition = conditionPrecompiled->getCondition();

        auto entries = m_table->select(key, condition);
        auto entriesPrecompiled = std::make_shared<EntriesPrecompiled>();
        entriesPrecompiled->setEntries(entries);

        auto newAddress = context->registerPrecompiled(entriesPrecompiled);
        out = abi.abiIn("", newAddress);
    }
    else if (func == name2Selector[TABLE_METHOD_INS_STR_ADD])
    {  // insert(string,address)
        std::string key;
        Address entryAddress;
        abi.abiOut(data, key, entryAddress);

        EntryPrecompiled::Ptr entryPrecompiled =
            std::dynamic_pointer_cast<EntryPrecompiled>(context->getPrecompiled(entryAddress));
        auto entry = entryPrecompiled->getEntry();
        checkLengthValidate(
            key, USER_TABLE_KEY_VALUE_MAX_LENGTH, CODE_TABLE_KEYVALUE_LENGTH_OVERFLOW);

        auto it = entry->begin();
        for (; it != entry->end(); ++it)
        {
            checkLengthValidate(
                it->second, USER_TABLE_FIELD_VALUE_MAX_LENGTH, CODE_TABLE_KEYVALUE_LENGTH_OVERFLOW);
        }
        int count = m_table->insert(key, entry, std::make_shared<AccessOptions>(origin));
        out = abi.abiIn("", u256(count));
    }
    else if (func == name2Selector[TABLE_METHOD_NEWCOND])
    {  // newCondition()
        auto condition = m_table->newCondition();
        auto conditionPrecompiled = std::make_shared<ConditionPrecompiled>();
        conditionPrecompiled->setCondition(condition);

        auto newAddress = context->registerPrecompiled(conditionPrecompiled);
        out = abi.abiIn("", newAddress);
    }
    else if (func == name2Selector[TABLE_METHOD_NEWENT])
    {  // newEntry()
        auto entry = m_table->newEntry();
        auto entryPrecompiled = std::make_shared<EntryPrecompiled>();
        entryPrecompiled->setEntry(entry);

        auto newAddress = context->registerPrecompiled(entryPrecompiled);
        out = abi.abiIn("", newAddress);
    }
    else if (func == name2Selector[TABLE_METHOD_RE_STR_ADD])
    {  // remove(string,address)
        std::string key;
        Address conditionAddress;
        abi.abiOut(data, key, conditionAddress);

        ConditionPrecompiled::Ptr conditionPrecompiled =
            std::dynamic_pointer_cast<ConditionPrecompiled>(
                context->getPrecompiled(conditionAddress));
        auto condition = conditionPrecompiled->getCondition();

        int count = m_table->remove(key, condition, std::make_shared<AccessOptions>(origin));
        out = abi.abiIn("", u256(count));
    }
    else if (func == name2Selector[TABLE_METHOD_UP_STR_2ADD])
    {  // update(string,address,address)
        std::string key;
        Address entryAddress;
        Address conditionAddress;
        abi.abiOut(data, key, entryAddress, conditionAddress);

        EntryPrecompiled::Ptr entryPrecompiled =
            std::dynamic_pointer_cast<EntryPrecompiled>(context->getPrecompiled(entryAddress));
        ConditionPrecompiled::Ptr conditionPrecompiled =
            std::dynamic_pointer_cast<ConditionPrecompiled>(
                context->getPrecompiled(conditionAddress));
        auto entry = entryPrecompiled->getEntry();

        auto it = entry->begin();
        for (; it != entry->end(); ++it)
        {
            checkLengthValidate(it->second, USER_TABLE_FIELD_VALUE_MAX_LENGTH,
                CODE_TABLE_FIELDVALUE_LENGTH_OVERFLOW);
        }
        auto condition = conditionPrecompiled->getCondition();

        int count = m_table->update(key, entry, condition, std::make_shared<AccessOptions>(origin));
        out = abi.abiIn("", u256(count));
    }
    else
    {
        STORAGE_LOG(ERROR) << LOG_BADGE("TablePrecompiled") << LOG_DESC("call undefined function!");
    }
    return out;
}

h256 TablePrecompiled::hash()
{
    return m_table->hash();
}
