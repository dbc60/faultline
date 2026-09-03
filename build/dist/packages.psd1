#
# See LICENSE.txt for copyright and licensing information about this file.
#
# What each service distribution package contains. This file is data only: the
# collecting logic lives in fl_dist.ps1, which reads this with
# Import-PowerShellDataFile and copies each listed file into dist\<name>\.
#
# Per package:
#   Version - recorded in manifest.txt; bump on release.
#           - Bump the minor version for changes to the file list.
#           - Bump the patch version for changes to file content.
#           - The major version will be bumped upon a stable release.
#   Depends - space-separated package names a consumer must import first
#   Src     - files from src\            -> dist\<name>\src\
#   Inc     - files from include\faultline\ -> dist\<name>\include\faultline\
#   Top     - files from include\        -> dist\<name>\include\
#   Fnv     - files from third_party\fnv\ -> dist\<name>\src\fnv\
#
# A package that ships private headers (any .h under Src) is reported to the
# caller with src\ as a second include path.
#
@{
    Packages = @{

        exception_service = @{
            Title   = 'Exception Service Distribution'
            Version = '0.2.2'
            Depends = ''

            Src = @(
                'fl_exception_service.c'
                'fla_exception_service.c'
                'flp_exception_service.c'
            )

            Inc = @(
                'fl_abbreviated_types.h'
                'fla_exception_service.h'
                'fl_exception_service.h'
                'fl_exception_service_assert.h'
                'fl_exception_types.h'
                'fl_macros.h'
                'fl_try.h'
            )

            Top = @(
                'flp_exception_service.h'
            )
        }

        fault_memory_service = @{
            Title   = 'Standalone Fault-Injecting Memory Service Distribution'
            Version = '0.2.3'
            Depends = ''

            Src = @(
                'arena.c'
                'arena_dbg.c'
                'arena_malloc.c'
                'arena_pool.c'
                'buffer.c'
                'digital_search_tree.c'
                'fl_exception_service.c'
                'fl_threads.c'
                'flp_exception_service.c'
                'flp_stream_service.c'
                'flp_log_service.c'
                'region.c'
                'region_node.c'
                'region_os.c'
                'region_windows.c'
                'lock_os.c'
                'win32_lock.c'
                'generic_lock.c'
                'set.c'
                'fault_injector.c'
                'fla_memory_service.c'
                'flp_memory_service.c'
                'flp_fault_memory_service.c'
                'arena_internal.h'
                'arena_dbg.h'
                'atomic.h'
                'bits.h'
                'chunk.h'
                'digital_search_tree.h'
                'index.h'
                'index_generic.h'
                'index_intel.h'
                'index_linux.h'
                'index_windows.h'
                'region.h'
                'region_node.h'
                'region_os.h'
                'region_windows.h'
                'fl_lock.h'
                'win32_platform.h'
                'fault_injector_internal.h'
                'fault_injector_resource.h'
            )

            Inc = @(
                'flp_memory_context.h'
                'flp_fault_memory_context.h'
                'arena.h'
                'arena_malloc.h'
                'arena_pool.h'
                'bitscan_windows.h'
                'dlist.h'
                'fl_abbreviated_types.h'
                'fl_exception_service.h'
                'fl_exception_service_assert.h'
                'fl_exception_types.h'
                'fl_stream_service.h'
                'fl_file_types.h'
                'fl_log.h'
                'fl_log_service.h'
                'fl_macros.h'
                'fl_threads.h'
                'fl_try.h'
                'size.h'
                'buffer.h'
                'fault.h'
                'fault_injector.h'
                'fault_site.h'
                'fl_memory.h'
                'fl_memory_service.h'
                'fla_memory_service.h'
                'fl_result_codes.h'
                'fl_test_summary.h'
                'fl_types.h'
                'set.h'
            )

            Top = @(
                'flp_memory_service.h'
                'flp_exception_service.h'
                'flp_stream_service.h'
                'flp_log_service.h'
            )

            Fnv = @(
                'FNV64.c'
                'FNV64.h'
                'FNVconfig.h'
                'FNVErrorCodes.h'
                'fnv-private.h'
            )
        }

        file_service = @{
            Title   = 'File Service Distribution'
            Version = '0.1.0'
            Depends = 'memory_service'

            Src = @(
                'flp_file_service.c'
                'fla_file_service.c'
            )

            Inc = @(
                'fl_file_types.h'
                'fl_file_service.h'
                'fla_file_service.h'
                'fl_file.h'
                'fl_async_file_service.h'
            )

            Top = @(
                'flp_file_service.h'
            )
        }

        log_service = @{
            Title   = 'Log Service Distribution'
            Version = '0.3.2'
            Depends = 'stream_service'

            Src = @(
                'fl_threads.c'
                'fl_lock.h'
                'lock_os.c'
                'win32_lock.c'
                'generic_lock.c'
                'flp_log_service.c'
                'fla_log_service.c'
            )

            Inc = @(
                'fla_log_service.h'
                'fl_log.h'
                'fl_log_service.h'
                'fl_macros.h'
                'fl_threads.h'
                'fl_try.h'
                'fl_exception_service_assert.h'
            )

            Top = @(
                'flp_log_service.h'
            )
        }

        memory_service = @{
            Title   = 'Standalone Memory Service Distribution (no fault injection)'
            Version = '0.2.3'
            Depends = ''

            Src = @(
                'arena.c'
                'arena_dbg.c'
                'arena_malloc.c'
                'arena_pool.c'
                'digital_search_tree.c'
                'region.c'
                'region_node.c'
                'region_os.c'
                'region_windows.c'
                'lock_os.c'
                'win32_lock.c'
                'generic_lock.c'
                'fl_exception_service.c'
                'flp_exception_service.c'
                'flp_stream_service.c'
                'flp_log_service.c'
                'fl_threads.c'
                'fla_memory_service.c'
                'flp_memory_service.c'
                'arena_internal.h'
                'arena_dbg.h'
                'atomic.h'
                'bits.h'
                'chunk.h'
                'digital_search_tree.h'
                'index.h'
                'index_generic.h'
                'index_intel.h'
                'index_linux.h'
                'index_windows.h'
                'region.h'
                'region_node.h'
                'region_os.h'
                'region_windows.h'
                'fl_lock.h'
                'win32_platform.h'
            )

            Inc = @(
                'flp_memory_context.h'
                'arena.h'
                'arena_malloc.h'
                'arena_pool.h'
                'bitscan_windows.h'
                'dlist.h'
                'fl_abbreviated_types.h'
                'fl_exception_service.h'
                'fl_exception_service_assert.h'
                'fl_exception_types.h'
                'fl_stream_service.h'
                'fl_file_types.h'
                'fl_log.h'
                'fl_log_service.h'
                'fl_macros.h'
                'fl_threads.h'
                'fl_try.h'
                'size.h'
                'fl_memory.h'
                'fl_memory_service.h'
                'fla_memory_service.h'
            )

            Top = @(
                'flp_memory_service.h'
                'flp_exception_service.h'
                'flp_stream_service.h'
                'flp_log_service.h'
            )
        }

        stream_service = @{
            Title   = 'Stream Service Distribution'
            Version = '0.1.0'
            Depends = 'memory_service'

            Src = @(
                'flp_stream_service.c'
                'fla_stream_service.c'
            )

            Inc = @(
                'fl_file_types.h'
                'fl_stream_service.h'
                'fla_stream_service.h'
                'fl_stream.h'
            )

            Top = @(
                'flp_stream_service.h'
            )
        }

        test_framework = @{
            Title   = 'Test Framework Distribution'
            Version = '0.3.0'
            Depends = 'exception_service timer_service'

            Inc = @(
                'fl_test.h'
                'fl_abi.h'
                'fl_case_outcome.h'
                'fl_types.h'
                'fl_threads.h'
            )
        }

        timer_service = @{
            Title   = 'Timer Service Distribution'
            Version = '0.1.0'
            Depends = 'exception_service'

            Src = @(
                'flp_timer_service.c'
                'fla_timer_service.c'
            )

            Inc = @(
                'fl_timer_service.h'
                'fla_timer_service.h'
                'fl_timer.h'
                'fl_stopwatch.h'
            )

            Top = @(
                'flp_timer_service.h'
            )
        }

    }
}
