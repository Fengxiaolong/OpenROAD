# exit on failure of any test line
set -e
docker run  -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenROAD openroad/openroad bash -c "/OpenROAD/test/regression fast"
docker run  -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenROAD openroad/openroad bash -c "/OpenROAD/src/resizer/test/regression fast"
docker run  -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenROAD openroad/openroad bash -c "/OpenROAD/src/opendp/test/regression fast"
docker run  -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenROAD openroad/openroad bash -c "cd /OpenROAD/src/replace/test && python3 regression.py run openroad"
docker run  -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenROAD openroad/openroad bash -c "cd /OpenROAD && cp etc/PO* . && tclsh ./src/FastRoute/tests/run_all.tcl"
docker run  -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):/OpenROAD openroad/openroad bash -c "cd /OpenROAD/src/OpenPhySyn/tests && PSN_TRANSFORM_PATH=../../../build/transforms ../../../build/src/OpenPhySyn/unit_tests"