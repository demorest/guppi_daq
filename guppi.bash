#!/bin/bash
# Set environment variables for GUPPI, bash version
echo "This script is specific to Green Bank."
echo "Setting GUPPI_DIR, PATH, PYTHONPATH, LD_LIBRARY_PATH, TEMPO, PRESTO and PGPLOT_DIR for GUPPI..."
OPT64=/opt/64bit
export GUPPI=$OPT64/guppi
export GUPPI_DIR=$GUPPI/guppi_daq
export PRESTO=$OPT64/presto
export PATH=$OPT64/bin:$GUPPI_DIR/bin:$GUPPI/bin:$PRESTO/bin:$PATH
export PYTHONPATH=$OPT64/lib/python:$OPT64/lib/python/site-packages:$OPT64/presto/lib/python:$GUPPI/lib/python/site-packages:$GUPPI/lib/python:$GUPPI_DIR/python
export PGPLOT_DIR=$OPT64/pgplot
export LD_LIBRARY_PATH=$OPT64/lib:$OPT64/pgplot:$OPT64/presto/lib
export TEMPO=$OPT64/tempo
