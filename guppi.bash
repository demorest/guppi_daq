# Set environment variables for GUPPI, bash version

# Check for an existing GUPPI_DIR, if it's there we don't
# do anything.
if test "${GUPPI_DIR}zz" = zz; then
	opt64=/opt/64bit
	export GUPPI_DIR=$opt64/guppi/guppi_daq
	export PATH=$opt64/bin:$GUPPI_DIR/bin:$PATH
	export PYTHONPATH=$opt64/lib/python:$opt64/lib/python/site-packages:$opt64/presto/lib/python:$GUPPI_DIR/python
	export LD_LIBRARY_PATH=$opt64/lib:$opt64/pgplot:$opt64/presto/lib
fi
