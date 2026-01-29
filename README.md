# Calendar Web App
A simple calendar app running off of a simple http server written in C. Post messages are also handled in C and compiled into a nice program.

It is imperfect mainly due to the JS which will need to be redone at some point.

# Building
The first time you build it, questions will be asked to make a cert and key for the HTTP server.
Normal:
```bash
make
```
Debug:
```bash
make MODE=debug
```

# Issues
If the http is not starting, try changing the port in server_settings to something not used (example, 80 to 8080).

If the server starts, but you can't connect, make sure you are navigating to https and not http. Some browsers will default to http because the certs are self signed.

# License
This project is licensed under the GNU Lesser General Public License v3.0 (LGPL-3.0-or-later).

You may link against this library in proprietary software, but any modifications to the library itself must be released under the same license.
