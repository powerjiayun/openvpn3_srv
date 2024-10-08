//    OpenVPN -- An application to securely tunnel IP networks
//               over a single port, with support for SSL/TLS-based
//               session authentication and key exchange,
//               packet encryption, packet authentication, and
//               packet compression.
//
//    Copyright (C) 2012-2022 OpenVPN Inc.
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Affero General Public License Version 3
//    as published by the Free Software Foundation.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public License
//    along with this program in the COPYING file.
//    If not, see <http://www.gnu.org/licenses/>.

// Select appropriate OpenVPN protocol data channel implementation

#ifndef OPENVPN_CRYPTO_CRYPTODCSEL_H
#define OPENVPN_CRYPTO_CRYPTODCSEL_H

#include <openvpn/common/exception.hpp>
#include <openvpn/crypto/cryptodc.hpp>
#include <openvpn/crypto/crypto_chm.hpp>
#include <openvpn/crypto/crypto_aead.hpp>
#include <openvpn/random/randapi.hpp>

namespace openvpn {

OPENVPN_EXCEPTION(crypto_dc_select);

/**
 * Implements the data channel encryption and decryption in userspace
 */
template <typename CRYPTO_API>
class CryptoDCSelect : public CryptoDCFactory
{
  public:
    typedef RCPtr<CryptoDCSelect> Ptr;

    CryptoDCSelect(SSLLib::Ctx libctx_arg,
                   const Frame::Ptr &frame_arg,
                   const SessionStats::Ptr &stats_arg,
                   const StrongRandomAPI::Ptr &rng_arg)
        : frame(frame_arg),
          stats(stats_arg),
          rng(rng_arg),
          libctx(libctx_arg)
    {
    }

    CryptoDCContext::Ptr new_obj(CryptoDCSettingsData dc_settings) override
    {
        const CryptoAlgs::Alg &alg = CryptoAlgs::get(dc_settings.cipher());
        if (alg.flags() & CryptoAlgs::CBC_HMAC)
            return new CryptoContextCHM<CRYPTO_API>(libctx, std::move(dc_settings), frame, stats, rng);
        else if (alg.flags() & CryptoAlgs::AEAD)
            return new AEAD::CryptoContext<CRYPTO_API>(libctx, std::move(dc_settings), frame, stats);
        else
            OPENVPN_THROW(crypto_dc_select, alg.name() << ": only CBC/HMAC and AEAD cipher modes supported");
    }

  private:
    Frame::Ptr frame;
    SessionStats::Ptr stats;
    StrongRandomAPI::Ptr rng;
    SSLLib::Ctx libctx;
};

} // namespace openvpn

#endif
