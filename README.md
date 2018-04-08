# 8005-asn3

## Building

the project uses a submodule for wrappers so it can be fetched at clone with

```
git clone --recurse-submodules https://github.com/isaacmorneau/8005-asn3.git
```

to keep the root folder orderly it is recomended to make from within bit as follows

```
mkdir 8005-asn3/bin
cd bin
cmake ../
make
```

## Running

After it is built running the program with no arguments will give the following output.
This page includes valid options as well as a couple examples

NOTE: `-p example.com@80:80` is functionally identical to ommiting the duplicate port such as `-p example.com@80`

```
usage options:
	-[-p]airs <address@inport[:outport]> - multiple flags for each forwarding pair
		-p example.com@3000:80 - redirects traffic locally from port 3000 to port 80 on example.com
	-[-t]cp - switch to tcp pairs(default)
	-[-u]dp - switch to udp pairs
	-[-h]elp - this message


example usage:
	-Forward ssh traffic from port 2000 to local port 22
	./8005-asn3 -p localhost@2000:22
	-Forward http traffic to a different site
	./8005-asn3 -p example.com@80
```
