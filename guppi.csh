# Set environment variables for GUPPI, bash version
echo "This script is specific to Green Bank."
echo "Setting GUPPI, PATH, PYTHONPATH, LD_LIBRARY_PATH, TEMPO, PRESTO and PGPLOT_DIR for GUPPI..."
set OPT64=/opt/64bit
setenv GUPPI $OPT64/guppi
setenv GUPPI_DIR $GUPPI/guppi_daq
setenv PRESTO $OPT64/presto
setenv PATH $OPT64/bin:$GUPPI_DIR/bin:$GUPPI/bin:$PRESTO/bin:$PATH
setenv PYTHONPATH $OPT64/lib/python:$OPT64/lib/python/site-packages:$OPT64/presto/lib/python:$GUPPI/lib/python/site-packages:$GUPPI/lib/python:$GUPPI_DIR/python
setenv PGPLOT_DIR $OPT64/pgplot
setenv LD_LIBRARY_PATH $OPT64/lib:$OPT64/pgplot:$OPT64/presto/lib
setenv TEMPO $OPT64/tempo
