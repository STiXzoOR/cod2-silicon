import Foundation
import CryptoKit
import Darwin

/// Reads a supplied raw Ed25519 key. No key generation, trust bootstrapping or
/// publication: a maintainer must establish those separately.
@main struct DirectorySign {
    static func main() {
        do {
            let args = CommandLine.arguments
            guard args.count == 5, ["sign", "verify"].contains(args[1]) else { throw CocoaError(.fileReadInvalidFileName) }
            let bytes = try Data(contentsOf: URL(fileURLWithPath: args[2]))
            let keyBytes = try Data(contentsOf: URL(fileURLWithPath: args[3]))
            let signatureURL = URL(fileURLWithPath: args[4])
            if args[1] == "sign" {
                let key = try Curve25519.Signing.PrivateKey(rawRepresentation: keyBytes)
                try key.signature(for: bytes).write(to: signatureURL, options: .atomic)
            } else {
                let key = try Curve25519.Signing.PublicKey(rawRepresentation: keyBytes)
                guard key.isValidSignature(try Data(contentsOf: signatureURL), for: bytes) else { throw CocoaError(.fileReadCorruptFile) }
            }
        } catch {
            // Never include file contents or private key bytes in a failure log.
            FileHandle.standardError.write(Data("Directory signature operation failed.\n".utf8)); exit(1)
        }
    }
}
