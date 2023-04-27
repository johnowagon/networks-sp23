urls="http://netsys.cs.colorado.edu/
http://netsys.cs.colorado.edu/css/style.css
http://netsys.cs.colorado.edu/fancybox/blank.gif
http://netsys.cs.colorado.edu/fancybox/fancybox.png
http://netsys.cs.colorado.edu/fancybox/fancybox-x.png
http://netsys.cs.colorado.edu/fancybox/fancybox-y.png
http://netsys.cs.colorado.edu/fancybox/fancy_title_over.png
http://netsys.cs.colorado.edu/fancybox/jquery.fancybox-1.3.4.css
http://netsys.cs.colorado.edu/fancybox/jquery.fancybox-1.3.4.pack.js
http://netsys.cs.colorado.edu/fancybox/jquery.mousewheel-3.0.4.pack.js
http://netsys.cs.colorado.edu/files/text1.txt
http://netsys.cs.colorado.edu/graphics/html.gif
http://netsys.cs.colorado.edu/graphics/mov.gif
http://netsys.cs.colorado.edu/graphics/pdf.gif
http://netsys.cs.colorado.edu/graphics/txt.gif
http://netsys.cs.colorado.edu/images/apple_ex.png
http://netsys.cs.colorado.edu/images/exam.gif
http://netsys.cs.colorado.edu/images/wine3.jpg
http://netsys.cs.colorado.edu/index.html
http://netsys.cs.colorado.edu/jquery-1.4.3.min.js"

numurls=20
proxy="http://127.0.0.1:5001"

aria2c -j $numurls --all-proxy=$proxy -Z $urls
