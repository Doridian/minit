# Basics

Minimalistic init system for LXC-like systems.

Runs process specified in CWD specified with UID and GID specified. Keeps restarting the process once it exits

Make sure /minit on your LXC is pointing to the folder with the minit binary and other files in this repo in it (readonly mountpoint is fine)

# Operation

On startup, runs /minit/load, which is expected to create /tmp/minit_services and /tmp/minit_onboot

After that, runs /tmp/minit_onboot (right now used to set up network interfaces and similar)

At last, it runs all services as defined by /tmp/minit_services

Default behaviour of /minit/load is to copy from /minit/hosts/FQDN/services (and optionally onboot, failing back to /minit/onboot if it does not exist)

# Configuration

services
```
UID GID CWD COMMAND...
```
example
```
0 0 / /usr/sbin/nginx -c /etc/nginx.conf
```

load is just any executable script (default uses `#!/bin/sh` scripts)

onboot is just any executable script (default uses `#!/bin/sh` scripts)

# Proxmox example

`/etc/pve/lxc/101.conf`

```
...
lxc.init_cmd: /minit/minit
mp0: /mnt/minit,mp=/minit,ro=1
```
