# Set environment variables for GUPPI, bash version

# Check for an existing GUPPI_DIR, if it's there we don't
# do anything.
if (! $?GUPPI_DIR) then 
	set opt64=/opt/64bit
	setenv GUPPI_DIR $opt64/guppi/guppi_daq
	setenv PATH $opt64/bin:$GUPPI_DIR/bin:$PATH
	setenv PYTHONPATH $opt64/lib/python:$opt64/lib/python/site-packages:$opt64/presto/lib/python:$GUPPI_DIR/python
	setenv LD_LIBRARY_PATH $opt64/lib:$opt64/pgplot:$opt64/presto/lib
endif
