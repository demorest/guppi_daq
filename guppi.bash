# Set environment variables for GUPPI, bash version
echo "Setting GUPPI_DIR, PATH, PYTHONPATH, LD_LIBRARY_PATH, TEMPO, PRESTO and PGPLOT_DIR for GUPPI..."
opt64=/opt/64bit
export GUPPI_DIR=$opt64/guppi/guppi_daq
export PRESTO=$opt64/presto
export PATH=$opt64/bin:$GUPPI_DIR/bin:$PRESTO/bin:$PATH
export PYTHONPATH=$opt64/lib/python:$opt64/lib/python/site-packages:$opt64/presto/lib/python:$GUPPI_DIR/python:/data1/demorest/rpc/src
export PGPLOT_DIR=$opt64/pgplot
export LD_LIBRARY_PATH=$opt64/lib:$opt64/pgplot:$opt64/presto/lib
export TEMPO=$opt64/tempo
