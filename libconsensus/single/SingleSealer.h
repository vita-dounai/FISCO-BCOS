#pragma once
#include "SingleEngine.h"
#include <libconsensus/Sealer.h>
#include <algorithm>
#include <memory>
#include <string>

#define SINGLE_SEALER_LOG(LEVEL) LOG(LEVEL) << LOG_BADGE("CONSENSUS") << LOG_BADGE("SINGLESEALER")

namespace dev
{
namespace consensus
{
class SingleSealer : public Sealer
{
public:
    SingleSealer(std::shared_ptr<dev::p2p::P2PInterface> _service,
        std::shared_ptr<dev::txpool::TxPoolInterface> _txPool,
        std::shared_ptr<dev::blockchain::BlockChainInterface> _blockChain,
        std::shared_ptr<dev::sync::SyncInterface> _blockSync,
        std::shared_ptr<dev::blockverifier::BlockVerifierInterface> _blockVerifier,
        PROTOCOL_ID const& _protocolId, KeyPair const& _keyPair,
        dev::h512s const& _sealerList = dev::h512s())
      : Sealer(_txPool, _blockChain, _blockSync)
    {
        m_consensusEngine = std::make_shared<SingleEngine>(_service, _txPool, _blockChain,
            _blockSync, _blockVerifier, _protocolId, _keyPair, _sealerList);
        m_singleEngine = std::dynamic_pointer_cast<SingleEngine>(m_consensusEngine);

        std::string threadName = "SingleSealer-" + std::to_string(m_singleEngine->groupId());
        setName(threadName);

        auto iter = std::find(_sealerList.begin(), _sealerList.end(), _keyPair.pub());
        m_idx = iter - _sealerList.begin();
        SINGLE_SEALER_LOG(INFO) << LOG_DESC("SingleSealer") << LOG_KV("idx", m_idx);
    }
    void start() override
    {
        Sealer::start();
        m_singleEngine->start();
    }
    void stop() override
    {
        Sealer::stop();
        m_singleEngine->stop();
    }

protected:
    void handleBlock() override
    {
        resetSealingHeader(m_sealing.block.header());
        m_sealing.block.calTransactionRoot();

        SINGLE_SEALER_LOG(INFO) << LOG_DESC("[handleBlock]++++++++++++++++ Generating seal")
                                << LOG_KV("blockNumber", m_sealing.block.header().number())
                                << LOG_KV("txNum", m_sealing.block.getTransactionSize())
                                << LOG_KV("hash", m_sealing.block.header().hash().abridged());

        if (m_sealing.block.getTransactionSize() == 0)
        {
            SINGLE_SEALER_LOG(TRACE) << LOG_DESC("[handleBlock]Empty block will not be committed");
            reset();
            m_singleEngine->resetLastBlockTime();
            return;
        }

        bool succ = m_singleEngine->commit(m_sealing.block);
        if (!succ)
        {
            reset();
        }
    }
    bool shouldSeal() override { return Sealer::shouldSeal() && m_idx == 0; }
    void reset()
    {
        resetSealingBlock();
        m_signalled.notify_all();
        m_blockSignalled.notify_all();
    }

    bool reachBlockIntervalTime() override { return m_singleEngine->reachBlockIntervalTime(); }

private:
    std::shared_ptr<SingleEngine> m_singleEngine;
    dev::consensus::IDXTYPE m_idx;
};
}  // namespace consensus
}  // namespace dev