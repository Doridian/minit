# Basics

Minimalistic init system for container.

Produces a ~10kB main binary using ~4kB of RAM (probably less, but RAM is allocated in 4kB pages so it cannot use less).

Runs process specified in CWD specified with UID and GID specified. Keeps restarting the process once it exits.

# Operation

On startup, runs `/minit/onboot` (customizable executable, can be a shell script or anything else executable, errors are ignored) and `/minit/parser` (not customizable, comes with minit) which parses `/minit/services` to a binary format.

# Configuration

services
```
StopSignal User/UID Group/GID WorkingDir Command+Args...
```
example
```
0 0 0 / /usr/sbin/nginx -c /etc/nginx.conf
INT myservice myservice /home/myservice /home/service/myservice
```

If StopSignal is 0, minit will pass on whatever signal it gets to stop (INT, TERM, ...) through to the process. Otherwise it will pass on the signal given in StopSignal.

You can use both numeric signals and UIDs/GIDs as well as string representations.

`/minit/onboot` is just any executable script (can work with shebang lines)

# Proxmox example

`/etc/pve/lxc/101.conf`

```
...
lxc.init_cmd: /minit/minit
```
# Docker

https://hub.docker.com/r/doridian/alpine-minit/ is a minimal alpine image with minit (ship your own `/minit/services` and optionally `/minit/onboot`)
