The file content that we sent back should only be the bytes corresponding to the valid range, not the whole file.

Tiny Web server with Range support                                              
                                                                                
This is the home directory for the Tiny server, a web server used in            
the textbook CSAPP3e. Tiny uses the GET method to serve static content          
(text, HTML, GIF, JPG, mp3, mp4 files) out of ./ (working directory)            
and to serve dynamic content by running CGI programs out of ./cgi-bin.          
The default page is home.html (rather than index.html) so that we can           
view the contents of the directory from a browser.                              
                                                                                
Tiny is neither secure nor complete, but it gives students an idea of           
how a real Web Server works. Use for instructional purposes only.               
                                                                                
The compiles and runs cleanly on W204 machines or Vagrant that has Centos6.10   
instsalled, running Linux kernel 2.6.32 and gcc version 4.4.7.                  
                                                                                
To compile Tiny:                                                                
  Type "make"                                                                   
                                                                                
To run Tiny:                                                                    
  Type "tiny <port>" on the server machine,                                     
    e.g. "tiny 8080".                                                           
  Point your browser at Tiny:                                                   
    static content: http://<host>:8080                                          
    dynamic content: http://<host>:8080/cgi-bin/adder?1&2                       
                                                                                
For debugging purposes, you should consider using command line browser curl.    
It has options (-v for verbose) where it will print out debugging               
info about the connections it makes to the server, the request headers it       
sends to the server and the response headers it receives from the webserver.    
  curl -v http://<host>:8080                                                    
                                                                                
                                                                                
Files:                                                                          
  csapp.c               Provide Rio package and other supporting functions      
  csapp.h               csapp supporting functions' declarations                
  tiny.c                The Tiny server                                         
  Makefile              Makefile for tiny.c                                     
  home.html             Test HTML page                                          
  godzilla.gif          Image embedded in home.html                             
  godzilla.jpg          jpg image                                               
  shaun.mp4             mp4 video file                                          
  sample.mp3            mp3 music file                                          
  range.txt             a text file for testing range support                   
  port-for-user.pl      "./port-for-user.pl userid" will give you an even port  
                        number p that won't easily collide with other students, 
                        you can use port p and p+1 for your experiment on W204. 
  free-port.sh          "./free-port.sh" will give you an unused port number.   
  README                This file                                               
  cgi-bin/adder.c       CGI program that adds two numbers                       
  cgi-bin/Makefile      Makefile for adder.c              
