#ifndef JWT_HPP
#define JWT_HPP

#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <ostream>

#include "jwt/base64.hpp"
#include "jwt/string_view.hpp"
#include "jwt/detail/meta.hpp"
#include "jwt/json/json.hpp"

// For convenience
using json_t = nlohmann::json;

namespace jwt {

/*!
 * JWT signing algorithm.
 */
enum class algorithm
{
  NONE = 0,
  HS256,
  HS384,
  HS512,
  RS256,
  RS384,
  RS512,
  ES256,
  ES384,
  ES512,
  TERM,
};


/*!
 */
string_view alg_to_str(enum algorithm alg) noexcept
{
  switch (alg) {
    case algorithm::HS256: return "HS256";
    case algorithm::HS384: return "HS384";
    case algorithm::HS512: return "HS512";
    case algorithm::RS256: return "RS256";
    case algorithm::RS384: return "RS384";
    case algorithm::RS512: return "RS512";
    case algorithm::ES256: return "ES256";
    case algorithm::ES384: return "ES384";
    case algorithm::ES512: return "ES512";
    case algorithm::TERM:  return "TERM";
    case algorithm::NONE:  return "NONE";
    default:               assert (0 && "Unknown Algorithm");
  };

  assert (0 && "Code not reached");
}


/*!
 */
enum class type
{
  JWT = 0,
};


/*!
 */
string_view type_to_str(enum type typ)
{
  switch (typ) {
    case type::JWT: return "JWT";
    default:        assert (0 && "Unknown type");
  };

  assert (0 && "Code not reached");
}


/*!
 * Registered claim names.
 */
enum class registered_claims
{
  // Expiration Time claim
  expiration = 0,
  // Not Before Time claim
  not_before,
  // Issuer name claim
  issuer,
  // Audience claim
  audience,
  // Issued At Time claim 
  issued_at,
  // Subject claim
  subject,
  // JWT ID claim
  jti,
};


/*!
 */
string_view reg_claims_to_str(enum registered_claims claim) noexcept
{
  switch (claim) {
    case registered_claims::expiration: return "exp";
    case registered_claims::not_before: return "nbf";
    case registered_claims::issuer:     return "iss";
    case registered_claims::audience:   return "aud";
    case registered_claims::issued_at:  return "iat";
    case registered_claims::subject:    return "sub";
    case registered_claims::jti:        return "jti";
    default:                            assert (0 && "Not a registered claim");
  };

  assert (0 && "Code not reached");
}

// Fwd declaration for friend functions to specify the 
// default arguments
// See: https://stackoverflow.com/a/23336823/434233
template <typename T>
std::string to_json_str(const T& obj, bool pretty=false);

template <typename T>
std::ostream& write(std::ostream& os, const T& obj, bool pretty=false);

/*!
 */
struct write_interface
{
  /*!
   */
  template <typename T>
  friend std::string to_json_str(const T& obj, bool pretty);

  /*!
   */
  template <typename T>
  friend std::ostream& write(
      std::ostream& os, const T& obj, bool pretty);

  /*!
   */
  template <typename T>
  friend std::ostream& operator<< (std::ostream& os, const T& obj);
};

/*!
 * Provides the functionality for doing
 * base64 encoding and decoding from the
 * json string.
 */
template <typename Derived>
struct base64_enc_dec
{
  /*!
   */
  std::string base64_encode(bool with_pretty = false) const
  {
    std::string jstr = to_json_str(*static_cast<const Derived*>(this), with_pretty);
    return jwt::base64_encode(jstr.c_str(), jstr.length());
  }

  /*!
   */
  static std::string base64_decode(const std::string& encoded_str)
  {
    return jwt::base64_decode(encoded_str.c_str(), encoded_str.length());
  }

};


/*!
 * JWT Header.
 */
struct jwt_header: write_interface
                 , base64_enc_dec<jwt_header>
{
public: // 'tors
  /*!
   */
  jwt_header() = default;

  /*!
   */
  jwt_header(enum algorithm alg, enum type typ = type::JWT)
    : alg_(alg)
    , typ_(typ)
  {
  }

  /// Default Copy and assignment
  jwt_header(const jwt_header&) = default;
  jwt_header& operator=(const jwt_header&) = default;

  ~jwt_header() = default;

public: // Exposed APIs
  /*!
   * NOTE: Any previously saved json dump or the encoding of the
   * header would not be valid after modifying the algorithm.
   */
  void algo(enum algorithm alg) noexcept
  {
    alg_ = alg;
  }

  /*!
   */
  enum algorithm algo() const noexcept
  {
    return alg_;
  }

  /*!
   * NOTE: Any previously saved json dump or the encoding of the
   * header would not be valid after modifying the type.
   */
  void typ(enum type typ) noexcept
  {
    typ_ = typ;
  }

  /*!
   */
  enum type typ() const noexcept
  {
    return typ_;
  }

  /*!
   */
  json_t create_json_obj() const
  {
    json_t obj = json_t::object();
    //TODO: should be able to do with string_view
    obj["typ"] = type_to_str(typ_).to_string();
    obj["alg"] = alg_to_str(alg_).to_string();

    return obj;
  }

private: // Data members
  /// The Algorithm to use for signature creation
  enum algorithm alg_ = algorithm::NONE;

  /// The type of header
  enum type      typ_ = type::JWT;
};


/*!
 * JWT Payload
 */
struct jwt_payload: write_interface
                  , base64_enc_dec<jwt_payload>
{
public: // 'tors
  /*!
   */
  jwt_payload() = default;

  /// Default copy and assignment operations
  jwt_payload(const jwt_payload&) = default;
  jwt_payload& operator=(const jwt_payload&) = default;

  ~jwt_payload() = default;

public: // Exposed APIs
  /*!
   */
  template <typename T>
  bool add_claim(const std::string& cname, T&& cvalue, bool overwrite=false)
  {
    // Duplicate claim names not allowed
    // if overwrite flag is set to true.
    auto itr = claim_names_.find(cname);
    if (itr != claim_names_.end() && !overwrite) {
      return false;
    }

    // Add it to the known set of claims
    claim_names_.emplace(cname.data(), cname.length());

    //Add it to the json payload
    //TODO: claim name copied twice inside json 
    //and in the set
    payload_[cname.data()] = std::forward<T>(cvalue);

    return true;
  }

  /*!
   */
  bool has_claim(const std::string& cname) const noexcept
  {
    return claim_names_.count(cname);
  }

  /*!
   */
  template <typename T>
  bool has_claim_with_value(const std::string& cname, T&& cvalue) const
  {
    auto itr = claim_names_.find(cname);
    if (itr == claim_names_.end()) return false;

    return (cvalue == payload_[cname]);
  }

  /*!
   */
  const json_t& create_json_obj() const
  {
    return payload_;
  }

private:
  /*!
   */
  struct case_compare {
    bool operator()(const std::string& lhs, const std::string& rhs) const
    {
      int ret = strcasecmp(lhs.c_str(), rhs.c_str());
      return (ret < 0);
    }
  };

  /// JSON object containing payload
  json_t payload_;
  /// The set of claim names in the payload
  std::set<std::string, case_compare> claim_names_;
};

/*!
 */
struct jwt_signature
{
};


/*!
 */
class jwt_object
{
};


} // END namespace jwt


#include "jwt/impl/jwt.ipp"

#endif
