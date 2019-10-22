#pragma once

#include <libblockchain/BlockChainInterface.h>
#include <libconsensus/ConsensusEngineBase.h>

#define SINGLE_ENGINE_LOG(LEVEL) LOG(LEVEL) << LOG_BADGE("CONSENSUS") << LOG_BADGE("SINGLEENGINE")

namespace dev
{
namespace consensus
{
class SingleEngine : public ConsensusEngineBase
{
public:
    SingleEngine(std::shared_ptr<dev::p2p::P2PInterface> _service,
        std::shared_ptr<dev::txpool::TxPoolInterface> _txPool,
        std::shared_ptr<dev::blockchain::BlockChainInterface> _blockChain,
        std::shared_ptr<dev::sync::SyncInterface> _blockSync,
        std::shared_ptr<dev::blockverifier::BlockVerifierInterface> _blockVerifier,
        PROTOCOL_ID const& _protocolId, KeyPair const& _keyPair,
        dev::h512s const& _sealerList = dev::h512s())
      : ConsensusEngineBase(_service, _txPool, _blockChain, _blockSync, _blockVerifier, _protocolId,
            _keyPair, _sealerList)
    {
        m_blockSync->registerConsensusVerifyHandler([](dev::eth::Block const&) { return true; });
        std::string threadName = "SingleEngine-" + std::to_string(m_groupId);
        setName(threadName);
    }

    void start() override
    {
        updateMaxBlockTransactions();
        m_accountType = dev::consensus::NodeAccountType::SealerAccount;
        ConsensusEngineBase::start();
        SINGLE_ENGINE_LOG(INFO) << LOG_DESC("[#start]Single engine started")
                             << LOG_KV("consensusStatus", consensusStatus());
    }

    bool commit(dev::eth::Block const& _block);
    void resetLastBlockTime() { m_lastBlockTime = dev::utcTime(); }
    bool reachBlockIntervalTime()
    {
        auto nowTime = dev::utcTime();
        return nowTime - m_lastBlockTime >= g_BCOSConfig.c_intervalBlockTime;
    }
    void reportBlock(dev::eth::Block const& _block) override;

private:
    void execBlock(Sealing& _sealing, dev::eth::Block const& _block);
    void checkAndSave(Sealing& _sealing);

    uint64_t m_lastBlockTime;
};
}  // namespace consensus
}  // namespace dev