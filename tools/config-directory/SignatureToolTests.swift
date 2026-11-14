import Foundation
import CryptoKit

@main struct SignatureToolTests {
    static func run(_ executable: URL, _ arguments: [String]) throws -> Int32 {
        let process = Process(); process.executableURL = executable; process.arguments = arguments
        process.standardOutput = FileHandle.nullDevice; process.standardError = FileHandle.nullDevice
        try process.run(); process.waitUntilExit(); return process.terminationStatus
    }
    static func main() throws {
        let folder = URL(fileURLWithPath: CommandLine.arguments[1])
        let signer = folder.appendingPathComponent("sign")
        let index = folder.appendingPathComponent("signature-test-index.json")
        let privateURL = folder.appendingPathComponent("fixture-private-key")
        let publicURL = folder.appendingPathComponent("fixture-public-key")
        let signature = folder.appendingPathComponent("fixture-index.sig")
        let key = Curve25519.Signing.PrivateKey()
        try key.rawRepresentation.write(to: privateURL)
        try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: privateURL.path)
        try key.publicKey.rawRepresentation.write(to: publicURL)
        let bytes = Data("{\"entries\":[],\"revision\":1,\"schemaVersion\":1}\n".utf8)
        try bytes.write(to: index)
        guard try run(signer, ["sign", index.path, privateURL.path, signature.path]) == 0,
              try run(signer, ["verify", index.path, publicURL.path, signature.path]) == 0 else { fatalError("FAIL: signing tool round trip") }
        try (bytes + Data([32])).write(to: index)
        guard try run(signer, ["verify", index.path, publicURL.path, signature.path]) != 0 else { fatalError("FAIL: tampered signing output") }
        try bytes.write(to: index)
        try Curve25519.Signing.PrivateKey().publicKey.rawRepresentation.write(to: publicURL)
        guard try run(signer, ["verify", index.path, publicURL.path, signature.path]) != 0 else { fatalError("FAIL: wrong tool public key") }
        let made = folder.appendingPathComponent("fixture-made-private"), madePublic = folder.appendingPathComponent("fixture-made-public")
        guard try run(signer, ["create", made.path]) == 0, try run(signer, ["public", made.path, madePublic.path]) == 0,
              (try FileManager.default.attributesOfItem(atPath: made.path)[.posixPermissions] as? Int) == 0o600,
              try run(signer, ["sign", index.path, made.path, signature.path]) == 0,
              try run(signer, ["verify", index.path, madePublic.path, signature.path]) == 0 else { fatalError("FAIL: created key round trip") }
        let first = try Data(contentsOf: made)
        guard try run(signer, ["create", made.path]) != 0, try Data(contentsOf: made) == first else { fatalError("FAIL: create replaced a key") }
        print("PASS: standalone Ed25519 signer/verifier round trip, tamper and wrong-key rejection, key creation that never replaces")
    }
}
