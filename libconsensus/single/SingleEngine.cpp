#include "SingleEngine.h"
#include <libconfig/GlobalConfigure.h>

using namespace dev;
using namespace dev::consensus;
using namespace dev::eth;
using namespace dev::blockchain;

bool SingleEngine::commit(Block const& _block)
{
    SINGLE_ENGINE_LOG(DEBUG) << LOG_DESC("[#commit]Start to commit block");
    Sealing workingSealing;
    try
    {
        execBlock(workingSealing, _block);
    }
    catch (std::exception& e)
    {
        SINGLE_ENGINE_LOG(WARNING) << LOG_DESC("[#checkAndExecute]Block execute failed")
                                   << LOG_KV("EINFO", boost::diagnostic_information(e));
        return false;
    }

    checkAndSave(workingSealing);
    return true;
}

void SingleEngine::execBlock(Sealing& _sealing, Block const& _block)
{
    Block working_block(_block);
    SINGLE_ENGINE_LOG(DEBUG) << LOG_DESC("[#execBlock]")
                             << LOG_KV("number", working_block.header().number())
                             << LOG_KV("hash", working_block.header().hash().abridged());
    m_blockSync->noteSealingBlockNumber(working_block.header().number());
    _sealing.p_execContext = executeBlock(working_block);
    _sealing.block = working_block;
}

void SingleEngine::checkAndSave(Sealing& _sealing)
{
    // callback block chain to commit block
    CommitResult ret = m_blockChain->commitBlock(_sealing.block, _sealing.p_execContext);
    if (ret == CommitResult::OK)
    {
        SINGLE_ENGINE_LOG(DEBUG) << LOG_DESC("[#checkAndSave]Commit block succ");
        // drop handled transactions
        dropHandledTransactions(_sealing.block);
    }
    else
    {
        SINGLE_ENGINE_LOG(ERROR) << LOG_DESC("[#checkAndSave]Commit block failed")
                                 << LOG_KV("highestNum", m_highestBlock.number())
                                 << LOG_KV("sealingNum", _sealing.block.blockHeader().number())
                                 << LOG_KV("sealingHash",
                                        _sealing.block.blockHeader().hash().abridged());
        /// note blocksync to sync
        // m_blockSync->noteSealingBlockNumber(m_blockChain->number());
        m_txPool->handleBadBlock(_sealing.block);
    }
}

void SingleEngine::reportBlock(dev::eth::Block const& _block)
{
    if (m_blockChain->number() == 0 || m_highestBlock.number() < _block.blockHeader().number())
    {
        m_lastBlockTime = utcTime();
        m_highestBlock = _block.blockHeader();
        updateMaxBlockTransactions();

        SINGLE_ENGINE_LOG(INFO) << LOG_DESC("[#reportBlock]^^^^^^^^Report Block")
                                << LOG_KV("number", m_highestBlock.number())
                                << LOG_KV("sealer", m_highestBlock.sealer())
                                << LOG_KV("hash", m_highestBlock.hash().abridged())
                                << LOG_KV("next", m_consensusBlockNumber)
                                << LOG_KV("txNum", _block.getTransactionSize())
                                << LOG_KV("blockTime", m_lastBlockTime);
    }
}