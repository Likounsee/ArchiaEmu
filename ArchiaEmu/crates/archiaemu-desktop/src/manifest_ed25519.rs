#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Production Ed25519 verification adapter for signed module catalogue envelopes.
//!
//! This verifies signatures against a public key supplied by the caller. It does
//! not decide which keys are trusted; callers must pin approved public keys in the
//! application and must not accept keys fetched from the same untrusted catalogue.

use archiaemu_core::module_manifest::ModuleManifest;
use archiaemu_core::module_manifest_trust::{
    ManifestSignatureVerifier, ManifestTrustError, ManifestTrustPolicy, SignatureVerificationError,
    SignedManifestEnvelope,
};
use ed25519_dalek::{Signature, VerifyingKey};

/// Ed25519 signature verifier backed by the reviewed ed25519-dalek implementation.
#[derive(Clone, Copy, Debug, Default)]
pub struct Ed25519ManifestVerifier;

impl ManifestSignatureVerifier for Ed25519ManifestVerifier {
    fn verify(
        &self,
        public_key: &[u8],
        payload: &[u8],
        signature: &[u8],
    ) -> Result<(), SignatureVerificationError> {
        let key_bytes: &[u8; 32] = public_key
            .try_into()
            .map_err(|_| SignatureVerificationError)?;
        let key = VerifyingKey::from_bytes(key_bytes).map_err(|_| SignatureVerificationError)?;
        let signature =
            Signature::from_slice(signature).map_err(|_| SignatureVerificationError)?;
        key.verify_strict(payload, &signature)
            .map_err(|_| SignatureVerificationError)
    }
}

/// Verifies and parses a signed manifest using the production Ed25519 verifier.
///
/// The trust policy must be constructed from public keys pinned by the application,
/// not from key material supplied by the catalogue. The parser receives only the
/// exact payload bytes covered by the signature.
pub fn verify_signed_manifest<F>(
    policy: &mut ManifestTrustPolicy,
    envelope: &SignedManifestEnvelope,
    parse_verified_payload: F,
) -> Result<ModuleManifest, ManifestTrustError>
where
    F: FnOnce(&[u8]) -> Result<ModuleManifest, ()>,
{
    policy.verify_and_parse(envelope, &Ed25519ManifestVerifier, parse_verified_payload)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn decode_hex<const N: usize>(input: &str) -> [u8; N] {
        assert_eq!(input.len(), N * 2);
        let mut output = [0_u8; N];
        for (index, pair) in input.as_bytes().chunks_exact(2).enumerate() {
            let digit = |byte: u8| match byte {
                b'0'..=b'9' => Some(byte - b'0'),
                b'a'..=b'f' => Some(byte - b'a' + 10),
                _ => None,
            };
            output[index] = (digit(pair[0]).unwrap() << 4) | digit(pair[1]).unwrap();
        }
        output
    }

    // RFC 8032, section 7.1, test vector 1 (empty message).
    const PUBLIC_KEY: &str =
        "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
    const SIGNATURE: &str = concat!(
        "e5564300c360ac729086e2cc806e828a",
        "84877f1eb8e5d974d873e06522490155",
        "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"
    );

    #[test]
    fn accepts_rfc8032_valid_signature() {
        let key = decode_hex::<32>(PUBLIC_KEY);
        let signature = decode_hex::<64>(SIGNATURE);
        assert!(Ed25519ManifestVerifier
            .verify(&key, b"", &signature)
            .is_ok());
    }

    #[test]
    fn rejects_modified_payload() {
        let key = decode_hex::<32>(PUBLIC_KEY);
        let signature = decode_hex::<64>(SIGNATURE);
        assert!(Ed25519ManifestVerifier
            .verify(&key, b"tampered", &signature)
            .is_err());
    }

    #[test]
    fn rejects_malformed_key_and_signature_lengths() {
        assert!(Ed25519ManifestVerifier
            .verify(b"short", b"", &[0; 64])
            .is_err());
        assert!(Ed25519ManifestVerifier
            .verify(&[0; 32], b"", &[0; 63])
            .is_err());
    }
}
