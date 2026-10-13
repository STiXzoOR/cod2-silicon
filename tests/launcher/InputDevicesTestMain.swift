import Foundation

@main struct InputDevicesTestMain {
    @MainActor static func main() throws {
        try InputDevicesTests.run(CommandLine.arguments[1])
        if CommandLine.arguments.contains("--system-probe") { try InputDevicesTests.systemProbe() }
    }
}
