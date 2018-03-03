
* socks5 proxy server

 **** a development is pending because of the job seeking ****
 
 SYNOPSIS
  # launch two servers like so:
  
   $ ./esocks -s 0.0.0.0 -p 2020 -u 127.0.0.1 -j 4000 -k password --local
   $ ./esocks -s 0.0.0.0 -p2020 -k password
   
 FEATURE(s):
  * handling https and http both connections with polling APIs
  * bypassing firewall e.g, port filtering, packet inspections ...

 TODO: 
  * cache resolved addresses, so that we can avoid calling getaddrinfo many times
  * add proper encryptions to encrypt packet
  * support UDP connections in general
  
 BUILD
  It's simple. If you have built Libevent, you can simply `make`, and you have a
  binary file called `evsocks`. Maybe I should add `configure` script in the near future.

 MOTIVATION
  Why am doing I this? I just wanted to learn C programming language by building something
  practical! The code looks terrible but please be gentle and understand my pourpse.
  This is just my study.

 DEPENDENCY:
  * Libevent(http://libevent.org)

 LICENSE
  * MIT
