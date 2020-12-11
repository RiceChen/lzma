# LZMA

## 介绍

> LZMA，（Lempel-Ziv-Markov chain-Algorithm的缩写），是一个Deflate和LZ77算法改良和优化后的压缩算法。它使用类似于 LZ77 的字典编码机制，在一般的情况下压缩率比 bzip2 为高，用于压缩的可变字典最大可达4GB。

### 目录结构

| 名称 | 说明 |
| ---- | ---- |
| docs  |  |
| examples | 例子目录，压缩文件，解压文件 |
| inc  | 头文件目录 |
| src  | 源代码目录 |

## 许可证

> lzma package 遵循 LGPLv2.1 许可，详见 `LICENSE` 文件。

## 获取软件包

> 使用 lzma package 需要在 RT-Thread 的包管理器中选择它，具体路径如下：

```
RT-Thread online packages
    miscellaneous packages --->
        [*] LZMA: A compression library with high compression ratio  --->
            [ ] Enable using lzma sample (NEW) 
                Version (latest)  --->
```
- Enable using lzma sample： lzma实例代码
- Version： 软件包版本选择

然后让 RT-Thread 的包管理器自动更新，或者使用 `pkgs --update` 命令更新包到 BSP 中。

## 运行实例

> 该示例为一个简单的文件压缩和解压的例程，需要依赖文件系统，用到的命令有两个lzma 和lzmainfo, 其中lzma有两个选项 -c和 -d， -c命令压缩一个文件到另一个文件，-d命令解压一个文件到另一个文件。 lzmainfo用于显示压缩后文件信息。
> 使用方式：

``` 
msh />lzma -c rtthread.bin rtthread.bin.lzma
[lzma] lzma compress file success!
msh />
msh />lzma -d rtthread.bin.lzma rtthread.bin
[lzma] lzma decompress file success!
msh />
msh />lzmainfo rtthread.bin.lzma

rtthread.bin.lzma
Uncompressed size:              0 MB (155 bytes)
Dictionary size:                0 MB (65536 bytes)
Literal context bits (lc):      3
Literal pos bits (lp):          0
Number of pos bits (pb):        2

msh />
```

## 注意事项

> 说明：

## 联系方式 & 感谢

* 维护：加饭
* 主页：https://github.com/RiceChen/lzma