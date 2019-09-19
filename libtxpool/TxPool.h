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

/**
 * @brief : transaction pool
 * @file: TxPool.h
 * @author: yujiechen
 * @date: 2018-09-23
 */
#pragma once
#include "TransactionNonceCheck.h"
#include "TxPoolInterface.h"
#include <libblockchain/BlockChainInterface.h>
#include <libdevcore/ThreadPool.h>
#include <libdevcore/easylog.h>
#include <libethcore/Block.h>
#include <libethcore/Common.h>
#include <libethcore/Protocol.h>
#include <libethcore/Transaction.h>
#include <libp2p/P2PInterface.h>

using namespace dev::eth;
using namespace dev::p2p;

#define TXPOOL_LOG(LEVEL) LOG(LEVEL) << LOG_BADGE("TXPOOL")

namespace dev
{
namespace txpool
{
class TxPool;

struct TxPoolStatus
{
    size_t current;
    size_t dropped;
};

class TxPoolNonceManager
{
public:
};
struct transactionCompare
{
    bool operator()(dev::eth::Transaction const& _first, dev::eth::Transaction const& _second) const
    {
        return _first.importTime() <= _second.importTime();
    }
};
class TxPool : public TxPoolInterface, public std::enable_shared_from_this<TxPool>
{
public:
    TxPool(std::shared_ptr<dev::p2p::P2PInterface> _p2pService,
        std::shared_ptr<dev::blockchain::BlockChainInterface> _blockChain,
        PROTOCOL_ID const& _protocolId, uint64_t const& _limit = 102400)
      : m_service(_p2pService),
        m_blockChain(_blockChain),
        m_limit(_limit),
        m_protocolId(_protocolId)
    {
        assert(m_service && m_blockChain);
        if (m_protocolId == 0)
            BOOST_THROW_EXCEPTION(InvalidProtocolID() << errinfo_comment("ProtocolID must be > 0"));
        m_groupId = dev::eth::getGroupAndProtocol(m_protocolId).first;
        m_txNonceCheck = std::make_shared<TransactionNonceCheck>(m_blockChain);
        m_commonNonceCheck = std::make_shared<CommonTransactionNonceCheck>();
        m_callbackPool =
            std::make_shared<dev::ThreadPool>("txPoolCallback-" + std::to_string(_protocolId), 2);
    }
    void setMaxBlockLimit(unsigned const& limit) { m_txNonceCheck->setBlockLimit(limit); }
    unsigned const& maxBlockLimit() { return m_txNonceCheck->maxBlockLimit(); }
    virtual ~TxPool() { clear(); }

    /**
     * @brief submit a transaction through RPC/web3sdk
     *
     * @param _t : transaction
     * @return std::pair<h256, Address> : maps from transaction hash to contract address
     */
    std::pair<h256, Address> submit(dev::eth::Transaction& _tx) override;

    /**
     * @brief Remove transaction from the queue
     * @param _txHash: Remove bad transaction from the queue
     */
    bool drop(h256 const& _txHash) override;
    bool dropBlockTrans(dev::eth::Block const& block) override;
    bool handleBadBlock(dev::eth::Block const& block) override;
    /**
     * @brief Get top transactions from the queue
     *
     * @param _limit : _limit Max number of transactions to return.
     * @param _avoid : Transactions to avoid returning.
     * @param _condition : The function return false to avoid transaction to return.
     * @return Transactions : up to _limit transactions
     */
    dev::eth::Transactions topTransactions(uint64_t const& _limit) override;
    dev::eth::Transactions topTransactions(
        uint64_t const& _limit, h256Hash& _avoid, bool _updateAvoid = false) override;
    dev::eth::Transactions topTransactionsCondition(
        uint64_t const& _limit, dev::h512 const& _nodeId) override;

    /// get all transactions(maybe blocksync module need this interface)
    dev::eth::Transactions pendingList() const override;
    /// get current transaction num
    size_t pendingSize() override;

    /// @returns the status of the transaction queue.
    TxPoolStatus status() const override;

    /// protocol id used when register handler to p2p module
    virtual PROTOCOL_ID const& getProtocolId() const override { return m_protocolId; }
    void setTxPoolLimit(uint64_t const& _limit) { m_limit = _limit; }

    /// Set transaction is known by a node
    void setTransactionIsKnownBy(h256 const& _txHash, h512 const& _nodeId) override;

    /// Is the transaction is known by the node ?
    bool isTransactionKnownBy(h256 const& _txHash, h512 const& _nodeId) override
    {
        auto p = m_transactionKnownBy.find(_txHash);
        if (p == m_transactionKnownBy.end())
            return false;
        return p->second.find(_nodeId) != p->second.end();
    }
    void setTransactionsAreKnownBy(
        std::vector<dev::h256> const& _txHashVec, h512 const& _nodeId) override;
    /// Is the transaction is known by someone
    bool isTransactionKnownBySomeone(h256 const& _txHash) override;
    SharedMutex& xtransactionKnownBy() override { return x_transactionKnownBy; }

    /// verify and set the sender of known transactions of sepcified block
    void verifyAndSetSenderForBlock(dev::eth::Block& block) override;
    bool txExists(dev::h256 const& txHash) override;

    bool isFull() override
    {
        // UpgradableGuard l(m_lock);
        return m_txsQueue.size() >= m_limit;
    }

    dev::ThreadPool::Ptr callbackPool() { return m_callbackPool; }

protected:
    /**
     * @brief : submit a transaction through p2p, Verify and add transaction to the queue
     * synchronously.
     *
     * @param _tx : Trasnaction data.
     * @param _ik : Set to Retry to force re-addinga transaction that was previously dropped.
     * @return ImportResult : Import result code.
     */
    ImportResult import(dev::eth::Transaction& _tx, IfDropped _ik = IfDropped::Ignore) override;
    ImportResult import(bytesConstRef _txBytes, IfDropped _ik = IfDropped::Ignore) override;
    /// verify transcation
    virtual ImportResult verify(
        Transaction& trans, IfDropped _ik = IfDropped::Ignore, bool _needinsert = false);
    /// check nonce
    virtual bool isBlockLimitOrNonceOk(dev::eth::Transaction const& _ts, bool _needinsert) const;
    /// interface for filter check
    virtual u256 filterCheck(const Transaction&) const { return u256(0); };
    void clear();
    bool dropTransactions(dev::eth::Block const& block, bool needNotify = false);
    bool removeBlockKnowTrans(dev::eth::Block const& block);

private:
    dev::eth::LocalisedTransactionReceipt::Ptr constructTransactionReceipt(
        dev::eth::Transaction const& tx, dev::eth::TransactionReceipt const& receipt,
        dev::eth::Block const& block, unsigned index);

    bool removeTrans(h256 const& _txHash, bool needTriggerCallback = false,
        dev::eth::LocalisedTransactionReceipt::Ptr pReceipt = nullptr);
    bool insert(dev::eth::Transaction const& _tx);
    void removeTransactionKnowBy(h256 const& _txHash);
    bool inline txPoolNonceCheck(dev::eth::Transaction const& tx)
    {
        if (!m_commonNonceCheck->isNonceOk(tx, true))
        {
            TXPOOL_LOG(WARNING) << LOG_DESC("txPoolNonceCheck: check TxPool Nonce failed");
            return false;
        }
        return true;
    }

private:
    /// p2p module
    std::shared_ptr<dev::p2p::P2PInterface> m_service;
    std::shared_ptr<dev::blockchain::BlockChainInterface> m_blockChain;
    std::shared_ptr<TransactionNonceCheck> m_txNonceCheck;
    /// nonce check for txpool
    std::shared_ptr<CommonTransactionNonceCheck> m_commonNonceCheck;
    /// Max number of pending transactions
    uint64_t m_limit;
    mutable SharedMutex m_lock;
    /// protocolId
    PROTOCOL_ID m_protocolId;
    GROUP_ID m_groupId;
    /// transaction queue
    using TransactionQueue = std::set<dev::eth::Transaction, transactionCompare>;
    TransactionQueue m_txsQueue;
    std::unordered_map<h256, TransactionQueue::iterator> m_txsHash;
    /// hash of dropped transactions
    h256Hash m_dropped;
    /// Transaction is known by some peers
    mutable SharedMutex x_transactionKnownBy;
    std::unordered_map<h256, std::unordered_set<h512>> m_transactionKnownBy;

    dev::ThreadPool::Ptr m_callbackPool;
};
}  // namespace txpool
}  // namespace dev
