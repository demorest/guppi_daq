# Set environment variables for GUPPI, bash version
echo "Setting GUPPI_DIR, PATH, PYTHONPATH, LD_LIBRARY_PATH, TEMPO, PRESTO and PGPLOT_DIR for GUPPI..."
set opt64=/opt/64bit
setenv GUPPI_DIR $opt64/guppi/guppi_daq
setenv PRESTO $opt64/presto
setenv PATH $opt64/bin:$GUPPI_DIR/bin:$PRESTO/bin:$PATH
setenv PYTHONPATH $opt64/lib/python:$opt64/lib/python/site-packages:$opt64/presto/lib/python:$GUPPI_DIR/python:/data1/demorest/rpc/src
setenv PGPLOT_DIR $opt64/pgplot
setenv LD_LIBRARY_PATH $opt64/lib:$opt64/pgplot:$opt64/presto/lib
setenv TEMPO $opt64/tempo
