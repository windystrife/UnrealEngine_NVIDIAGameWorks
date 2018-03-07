This folder contains pre-compressed files ready for deployment, please make sure to use the shipping build for final deployment.

Files Required for Final Deployment
-----------------------------------

*.js.gz   - compressed JavaScript files.
*.data    - compressed game content.
*.mem     - compressed memory initialization file.
*.html    - uncompressed landing page.
*.symbols - uncompressed symbols, if necessary.


Local Testing
-------------

run HTML5Launcher.exe (via mono on Mac) to start a web sever which is configured to serve compressed files on localhost. This is not a production quality server.
add -ServerPort=XXXX to the command line if necessary to change the serving port. Default port is 8000.


How to Deploy on Apache
-----------------------

Add the following in .conf or .htaccess file. ( edit the directory name accordingly )

<Directory "${SRVROOT}/htdocs">
 AddEncoding gzip gz
</Directory>
