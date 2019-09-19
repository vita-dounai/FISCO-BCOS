/**
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
 *
 * @brief: simple benchmark for crypto algorithm
 *
 * @file: crypto_main.cpp
 * @author: catli
 * @date 2019-09-19
 */

#include <libdevcrypto/AES.h>
#include <boost/lexical_cast.hpp>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>

using namespace std;
using namespace std::chrono;
using namespace dev;
using namespace boost;

int main(int argc, const char* argv[])
{
    auto repeatCount = 10000000;
    if (argc > 1)
    {
        try
        {
            repeatCount = lexical_cast<decltype(repeatCount)>(argv[1]);
        }
        catch (bad_lexical_cast&)
        {
        }
    }
    // AES benchmark
    cout << "Testing AES encrypt/decrypt ..." << endl;
    auto key = string("0123456789ABCDEF");
    auto data = string("8299b6471f3d178583392668575997b476be5ca474beb98347244cfd784e72ad");

    auto encryptedData = string();
    auto startTime = utcTime();
    for (auto i = 0; i < repeatCount; ++i)
    {
        encryptedData = dev::aesCBCEncrypt(data, key);
    }
    auto endTime = utcTime();
    auto encryptCost = endTime - startTime;
    encryptCost = encryptCost == 0 ? 1 : encryptCost;
    cout << "Encrypt performance: " << repeatCount / ((double)encryptCost / 1000)
         << " times per second" << endl;

    startTime = utcTime();
    for (auto i = 0; i < repeatCount; ++i)
    {
        dev::aesCBCDecrypt(encryptedData, key);
    }
    endTime = utcTime();
    auto decrtyptCost = endTime - startTime;
    decrtyptCost = decrtyptCost == 0 ? 1 : decrtyptCost;
    cout << "Decrypt performance: " << repeatCount / ((double)decrtyptCost / 1000)
         << " times per second" << endl;

    return 0;
}
