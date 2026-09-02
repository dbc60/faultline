@ECHO OFF
:: See LICENSE.txt for copyright and licensing information about this file.

pushd test
ctime.exe -begin timed_test.ctm
.\faultline.exe run ^
    fl_exception_tests.dll ^
    but_tests.dll ^
    dlist_tests.dll ^
    fl_log_service_tests.dll ^
    bits_tests.dll ^
    region_tests.dll ^
    chunk_tests.dll ^
    digital_search_tree_tests.dll ^
    index_generic_tests.dll ^
    index_windows_tests.dll ^
    arena_tests.dll ^
    arena_pool_tests.dll ^
    buffer_tests.dll ^
    fnv_tests.dll ^
    math_tests.dll ^
    set_tests.dll ^
    timer_tests.dll ^
    fault_injector_tests.dll ^
    fl_assert_tests.dll ^
    command_tests.dll ^
    faultline_tests.dll ^
    malloc_cleanup_config_tests.dll ^
    flp_memory_service_tests.dll ^
    flp_file_service_tests.dll ^
    flp_stream_service_tests.dll
ctime.exe -end timed_test.ctm
popd
