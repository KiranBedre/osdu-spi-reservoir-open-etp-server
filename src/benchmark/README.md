# Open ETP Server benchmark
Based on [Google Benchmark framework](https://github.com/google/benchmark).
Serves to measure the performance of the library and individual components.

## Build and run
- Make sure that CMake option `BUILD_BENCHMARK` is `ON` (enabled by default)
- Build Open-ETP-Server as usually (better in `Release` configuration)
- Run `oesBenchmark` from your build directory

You can filter benchmarks which you would like to run via `--benchmark_filter=<regex>` option, like below:
```shell
oesBenchmark --benchmark_filter=Eml_Compressor

---------------------------------------------------------------------------------------------
Benchmark                                   Time             CPU   Iterations UserCounters...
---------------------------------------------------------------------------------------------
Eml_CompressorDeflate/zlib/level:0       53.4 us         54.2 us        12976 CompressRatio=25.2 Rate=113.29M/s
Eml_CompressorDeflate/zlib/level:1       81.2 us         78.5 us         8960 CompressRatio=21.9 Rate=78.2274M/s
Eml_CompressorDeflate/zlib/level:2       88.7 us         87.9 us         7467 CompressRatio=21.7 Rate=69.849M/s
Eml_CompressorDeflate/gzip/level:0       55.0 us         56.2 us        10000 CompressRatio=25.5 Rate=109.134M/s
Eml_CompressorDeflate/gzip/level:1       79.9 us         80.2 us         8960 CompressRatio=22.2 Rate=76.5268M/s
Eml_CompressorDeflate/gzip/level:2       94.6 us         94.2 us         7467 CompressRatio=22 Rate=65.1924M/s
Eml_CompressorInflate/zlib/level:0       12.9 us         12.6 us        56000 Rate=488.921M/s
Eml_CompressorInflate/zlib/level:1       9.51 us         9.63 us        74667 Rate=637.726M/s
Eml_CompressorInflate/zlib/level:2       9.26 us         9.21 us        74667 Rate=666.714M/s
Eml_CompressorInflate/gzip/level:0       12.0 us         12.0 us        64000 Rate=513.154M/s
Eml_CompressorInflate/gzip/level:1       10.7 us         10.9 us        56000 Rate=564.14M/s
Eml_CompressorInflate/gzip/level:2       10.8 us         10.9 us        74667 Rate=564.143M/s
```

All tests follow `<Area>_<Subject><Action>` naming pattern to simplify filtering.
Test results can be exported to file via option `--benchmark_format=<console|json|csv>`.
Get more information about the command line interface via `--help` option.

## Variance of results
Modern CPUs have a lot of hardware tricks for get best performance with minimal power consumption. All of them make it a bit difficult to get stable test results, please be patient, try running the test a few times.
You can get more information about this topic on [official page](https://github.com/google/benchmark/blob/main/docs/reducing_variance.md) of Google benchmark framework, there are described recommendations how to reduce the variance.

## High-load stability testing
The benchmark includes set of tests for simulate high-load client-server interaction:
- ClientHighLoad_ArrayGet
- Client_ConnectionEstablishMt
- Client_ConnectionPingMt
- Client_ArrayGetMt
- Client_ArrayPutSubPartMt
- Client_EpcExportMt

They will help to keep stability of library when refactoring code. You can temporary adjust the number of iterations in benchmarks for increase testing time, for example:
```cpp
BENCHMARK(Client_ArrayGetMt)->ThreadRange(1, 16)->Unit(kMillisecond)->Iterations(1000)->UseRealTime();
```

## Development of new benchmarks
Follow official [guideline](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
