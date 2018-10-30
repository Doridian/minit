# Basics

Minimalistic init system for containers.

Produces a ~10kB main binary using ~4kB of RAM (probably less, but RAM is allocated in 4kB pages so it cannot use less).

Runs process specified in CWD specified with UID and GID specified. Keeps restarting the process once it exits.

# Operation

On startup, runs `/etc/minit/onboot` (customizable executable, can be a shell script or anything else executable) and `/sbin/minit_parser` (not customizable, comes with minit) which parses `/etc/minit/services` to a binary format.

# Configuration

`services`

```
StopSignal User/UID Group/GID WorkingDir Command+Args...
```
example
```
TERM 0 0 / /usr/sbin/nginx -c /etc/nginx.conf
INT myservice myservice /home/myservice /home/myservice/bin
```

You can use both numeric signals and UIDs/GIDs as well as string representations.

`onboot` is just any executable script (can work with shebang lines)

# Proxmox example

`/etc/pve/lxc/101.conf`

```
...
lxc.init_cmd: /sbin/minit
```
# Docker

https://hub.docker.com/r/doridian/alpine-minit/ is a minimal alpine image with minit (ship your own `/etc/minit/services` and optionally `/etc/minit/onboot`)
