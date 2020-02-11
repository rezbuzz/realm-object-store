////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or utilied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef app_credentials_hpp
#define app_credentials_hpp

#include <stdio.h>
#include <string>

namespace realm {

typedef std::string IdentityProvider;
typedef std::string AppCredentialsToken;

/// The username/password identity provider. User accounts are handled by the Realm Object Server directly without the
/// involvement of a third-party identity provider.
extern IdentityProvider const IdentityProviderUsernamePassword;

/// A Facebook account as an identity provider.
extern IdentityProvider const IdentityProviderFacebook;

/// A Google account as an identity provider.
extern IdentityProvider const IdentityProviderGoogle;

/// A JSON Web Token as an identity provider.
extern IdentityProvider const IdentityProviderJWT;

/// An Anonymous account as an identity provider.
extern IdentityProvider const IdentityProviderAnonymous;

/// A Google account as an identity provider.
extern IdentityProvider const IdentityProviderApple;

enum class AuthProvider {
    ANONYMOUS,
    FACEBOOK,
    APPLE
};

IdentityProvider provider_type_from_enum(AuthProvider provider);

/**
 Opaque credentials representing a specific Realm Object Server user.
 */
struct AppCredentials {
public:
    /**
     Construct and return credentials from a Facebook account token.
     */
    static std::shared_ptr<AppCredentials> facebook(AppCredentialsToken access_token);

    /**
     Construct and return anonymous credentials
     */
    static std::shared_ptr<AppCredentials> anonymous();

    /**
     Construct and return credentials from an Apple account token.
     */
    static std::shared_ptr<AppCredentials> apple(AppCredentialsToken id_token);

    AuthProvider provider() const;
    AppCredentialsToken token() const;
    std::vector<char> serialize() const;
    
    ~AppCredentials() = default;

    AppCredentials() {};

    AppCredentials(AppCredentials&&) = default;
    AppCredentials& operator=(AppCredentials&&) = default;
    AppCredentials(AppCredentials const&) = delete;
    AppCredentials& operator=(AppCredentials const&) = delete;
private:
    /// An opaque credentials token containing information that uniquely identifies a Realm Object Server user.
    AppCredentialsToken m_token;

    /// The name of the identity provider which generated the credentials token.
    AuthProvider m_provider;

    friend class RealmApp;
};
}
#endif /* app_credentials_hpp */