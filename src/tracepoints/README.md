# LTTng Tracepoints

## Tracepoint Debian/Ubuntu configuration
There is a problem with embeddedSOME/IP and LTTng by default, because embeddedSOME/IP needs `sudo` to run without an error. By default LTTng can't read Tracepoints from a program that is executed with `sudo`. To solve the issue you can add a permission to your current user:

1. Check that the `tracing` group exists:
```console
getent group tracing
```

2. Add your user to the group:
```console
sudo usermod -aG tracing $USER
``` 

3. Re-login or restart the session:
```console
newgrp tracing
```


## View Tracepoints live

1. Install dependencies:

```console
sudo apt update
sudo apt install lttng-tools lttng-ust babeltrace2
```

2. Start an LTTng Session (name: my_session)
```console
lttng create my_session --live
```
Note: `--live` is only necessary if you want to view the Tracepoints will executing (live)

3. Enable a Userspace Event
```console
lttng enable-event -u lwip_receive
```

- u: Enables userpsace tracing
- lwip_receive: The event name (from the `LTTNG_UST_TRACEPOINT_EVENT`)

Alternative: Enable all userspace events:
```console
lttng enable-event -u -a
```

4.  Start Tracing
```console
lttng start
```

5. View Events Live
```console
lttng view
```

Alternative for more readable output:
```console
babeltrace2 -i lttng-live net://localhost
```
Addtion if you want to filter events while viewing:
```console
babeltrace2 -i lttng-live net://localhost | grep lwip_receive
```

6. Generate Events
Run a embeddedSOME/IP program, e.g. an example

7. Stop Tracing & Cleanup
```console
lttng stop
lttng destroy my_session
```

### Debugging
You can see all Tracepoints that are registered right now with the following command. (Note: Your program must run otherwise the Tracepoints are not registered!)
```console
lltng list -u
```