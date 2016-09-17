## Name

ssdb_with_lua


It's fork from [SSDB](https://github.com/ideawu/ssdb), and embed the power of lua into ssdb.


## Client API example

```lua
--add.lua
local ret = ssdb.get("key1")
ret = ret + 1
ssdb.set("key1", ret)
```

use the ssdb client:
lua_thread add.lua


## Compile and Install

```sh
$ wget --no-check-certificate https://github.com/ideawu/ssdb/archive/master.zip
$ unzip master
$ cd ssdb-master
$ make
$ #optional, install ssdb in /usr/local/ssdb
$ sudo make install

# start master
$ ./ssdb-server ssdb.conf

# or start as daemon
$ ./ssdb-server -d ssdb.conf

# ssdb command line
$ ./tools/ssdb-cli -p 8888

# stop ssdb-server
$ ./ssdb-server ssdb.conf -s stop
 # for older version
$ kill `cat ./var/ssdb.pid`
```

See [Compile and Install wiki](http://ssdb.io/docs/install.html)


## LICENSE

[New BSD License](http://opensource.org/licenses/BSD-3-Clause), a very flexible license to use.

