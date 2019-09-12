// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "block.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"
#include "blake2b.h"
#include "util.h"
#include "arith_uint256.h"

void FillBlockSolution(CBlock &block, uint32_t *solutions)
{
	block.nSolutions.clear();
	for(int i = 0; i < CBlock::SOLUTION_SIZE; i++) {
		block.nSolutions.push_back(*(solutions + i));
	}
}

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, nSolution = %u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce.ToString(),
        nSolutions.size(), vtx.size());

	if(nSolutions.size()) {
        s << "  Solutions: \n";
        s << "    ";
        for (const uint32_t sol : nSolutions) {
            s << strprintf("0x%08x, ", sol);
        }
        s << "\n";
    } else {
        s << "  Solutions: No\n";
    }

    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}

bool HashLessThan(const CBlock &block, const uint256 &target)
{
	return Uint256LessThan(block.GetHash(), target);
}
