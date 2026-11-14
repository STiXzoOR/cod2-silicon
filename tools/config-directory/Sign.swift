import Foundation
import CryptoKit
import Darwin

/// Signs and verifies with a raw Ed25519 key that the maintainer holds outside every repository.
///
///     sign   <file> <private key> <signature out>
///     verify <file> <public key>  <signature>
///     create <private key>                 a new key, readable by its owner only; never replaces one
///     public <private key> <public key out>
///
/// Publication and putting the public key into the launcher stay with the maintainer.
@main struct DirectorySign {
    static func main() {
        do {
            let args = CommandLine.arguments
            if args.count == 3, args[1] == "create" {
                guard !FileManager.default.fileExists(atPath: args[2]),
                      FileManager.default.createFile(atPath: args[2], contents: Curve25519.Signing.PrivateKey().rawRepresentation,
                                                     attributes: [.posixPermissions: 0o600]) else { throw CocoaError(.fileWriteFileExists) }
                return
            }
            if args.count == 4, args[1] == "public" {
                let key = try Curve25519.Signing.PrivateKey(rawRepresentation: Data(contentsOf: URL(fileURLWithPath: args[2])))
                try key.publicKey.rawRepresentation.write(to: URL(fileURLWithPath: args[3]), options: .atomic)
                return
            }
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
