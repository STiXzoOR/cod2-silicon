-- Create the single private oracle VM, or return the existing VM unchanged.
-- Usage: osascript tools/oracle/vm.applescript DEBIAN_QCOW2 CLOUD_INIT_ISO
-- Both input files stay private; never add them to Git. Resize the qcow2 to
-- 36 GiB before importing: UTM ignores guest size when source is provided.
-- The raw TCP enumerator avoids UTM's ambiguous tcp serial/network keywords.
-- SrPt avoids the serial-port element/property name collision in UTM 4.7.5.
-- Attach the seed as a fixed VirtIO disk so cloud kernels need no USB storage.
on run argv
    if (count of argv) is not 2 then error "Expected Debian qcow2 and cloud-init ISO paths"
    set diskImage to POSIX file (item 1 of argv) as alias
    set seedImage to POSIX file (item 2 of argv) as alias
    tell application "/Applications/UTM.app"
        set matches to every virtual machine whose name is "cod2-oracle"
        if (count of matches) > 1 then error "Multiple cod2-oracle VMs exist; refusing to create another"
        if (count of matches) is 1 then return id of (item 1 of matches)
        set oracle to make new virtual machine with properties {backend:qemu, configuration:{name:"cod2-oracle", architecture:"aarch64", machine:"virt", memory:8192, cpu cores:4, hypervisor:true, uefi:true, directory share mode:«constant QeSmSmOf», notes:"Private CoD2 Linux 1.3 replay oracle. 36 GiB disk; SSH only on localhost:22220.", drives:{{interface:VirtIO, source:diskImage}, {interface:VirtIO, source:seedImage}}, network interfaces:{{hardware:"virtio-net-pci", mode:emulated, port forwards:{{protocol:«constant NtPrTcPp», host address:"127.0.0.1", host port:22220, guest port:22}}}}, «property SrPt»:{{interface:ptty}}, displays:{}}}
        return id of oracle
    end tell
end run
