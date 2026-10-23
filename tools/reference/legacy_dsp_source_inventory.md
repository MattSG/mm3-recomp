# DSP source inventory - 2026-10-08

Pinned xemu commit: `478b4f496102379c7eaa7f3ec10e714a703c4300`. Upstream DSP sources were verified unchanged. Hashes record this snapshot and require refresh after edits.

Production `src/apu/CMakeLists.txt` links `apu_dsp.c`, `dsp/dsp.c`, `dsp/dsp_dma.c` and `dsp/interp/dsp_cpu.c` in static `xbox_apu`; interpreter includes provide the remaining implementation.

| Local source | Pinned upstream source | Header license | Local SHA-256 | Upstream SHA-256 |
|---|---|---|---|---|
| `dsp/interp/dsp_cpu.c` | `hw/xbox/mcpx/apu/dsp/interp/dsp_cpu.c` | GPL-2.0-or-later | `79a4b58944cac5a4f4a4a445af6fbcb129f9bd7616fdcf5fea64d139760c010e` | `b2879f6895ac5b4702a3a77dbd94fc5e4517928f5ef449030d44557860833e5b` |
| `dsp/interp/dsp_cpu.h` | `hw/xbox/mcpx/apu/dsp/interp/dsp_cpu.h` | GPL-2.0-or-later | `3d2ab2703f83120a63f7dbc767d0f117bc51f1b7a603b9a604610a5197ce0a52` | `314ab9afc8a01f9ceeafd6d0cb9a3e2446b0aa7e1bede758c5d7ad2b141163cf` |
| `dsp/interp/dsp_cpu_regs.h` | `hw/xbox/mcpx/apu/dsp/interp/dsp_cpu_regs.h` | GPL-2.0-or-later | `0d81d5f65b467632b8cd54f7aeee33caf96d6a5ef9c232390cb6ed591d0a327a` | `5373cbe8cb5326f39cbebfbbb13a85d78b4f8d47d7ebd3e4f2291e2fcde2c926` |
| `dsp/interp/dsp_emu.c.inc` | `hw/xbox/mcpx/apu/dsp/interp/dsp_emu.c.inc` | GPL-2.0-or-later | `8b4b9a3b6352bef5777295ab6c0e15d7285ad1383123f301fefa7db7cf556578` | `ffc744df58e3fea9df1fd2acffe267cb4126b5ecb6c82518009bcea17fbb6e5f` |
| `dsp/interp/dsp_dis.c.inc` | `hw/xbox/mcpx/apu/dsp/interp/dsp_dis.c.inc` | GPL-2.0-or-later | `de282c7323520789085be9d847385032bc0c953f77e746ec92901529b9f027e4` | `2a624a326c7633ff8a866bea4b379ac23aef3c3768a0873bf7d85a6b3e35911d` |
| `dsp/debug.h` | `hw/xbox/mcpx/apu/dsp/debug.h` | GPL-2.0-or-later | `1773c7f3b3db5318886ce48b01e42e00c94b80a8364089dc6276d6c7ed26eeef` | `1773c7f3b3db5318886ce48b01e42e00c94b80a8364089dc6276d6c7ed26eeef` |
| `dsp/dsp_dma.c` | `hw/xbox/mcpx/apu/dsp/dsp_dma.c` | LGPL-2.0-or-later | `438238fa9542fae5fc4bd1d8591c32db39213685c9839b41450faf228b741100` | `4a19ab8e1d53a8e58d2301f777d509fc5dbf19daa04deb194b25465ec97a9369` |
| `dsp/dsp_dma.h` | `hw/xbox/mcpx/apu/dsp/dsp_dma.h` | LGPL-2.0-or-later | `066dc72b21f884062efc8b42a53be84da19907be97bc235d546a583fb5634f41` | `117a7db6ae3b7bc7f8bf47f9dad30610dd72b0affba7ab28cdb721be3e4c6ffd` |
| `dsp/dsp_dma_regs.h` | `hw/xbox/mcpx/apu/dsp/dsp_dma_regs.h` | LGPL-2.0-or-later | `9582796ee42313de2f341356761bdef514b8a9f9962199df8881a7461b78ea62` | `9582796ee42313de2f341356761bdef514b8a9f9962199df8881a7461b78ea62` |
| `dsp/dsp.c` | `hw/xbox/mcpx/apu/dsp/dsp.c` | GPL-2.0-or-later | `769155e4c07cf28635deec2818c2401ef5cf3e3b20b619cfc00ac15ee854419e` | `bd40122dbca27b1ac8975f13786d071bed3a2380cb43bb368088a173d9794b60` |
| `apu_dsp.c` | `hw/xbox/mcpx/apu/dsp/gp_ep.c` | LGPL-2.0-or-later | `000f55e68b95eb2fd67d7391be40fec0e0251dd72273c5d12e063643f952040b` | `979044a337f3763b3eb079810f9182d6bc025529f30b76deb425a7bd18766c2e` |

`dsp.c` also adapts pinned upstream `hw/xbox/mcpx/apu/dsp/dsp_c.c`. Local `dsp.h` and `dsp_port.h` provide state declarations and host support, including incorporated GPL/LGPL interfaces. ISA corrections and provenance are documented in [PROVENANCE.md](PROVENANCE.md).

## License texts

- `COPYING.GPL-2` matches pinned upstream `COPYING` after line-ending normalization; SHA-256 `dd3ce02338c3a48abb6ba59b48809f7108a8bd242cb0cc8be90daafa30707c28`.
- `COPYING.LGPL-2.1` matches pinned upstream `COPYING.LIB` after line-ending normalization; SHA-256 `31c90ce76b6f5aab90a205851e71d5c27e31c0aa3d7017a4383b98a6fe3f1faa`.

The supplied LGPL text is version 2.1; per-file headers specify version 2 or later. The GPL interpreter is not covered solely by the parent MIT notice. This records source terms and linkage; no distribution package has been audited.

The pinned reference CPU is unchanged, but full-frame reference replay shares adapted local DMA/peripheral code. Its agreement is not independent full-xemu APU equivalence.

Additional upstream `dsp_c.c` SHA-256: `89ed49bf56f66e8b59ae170911ecc9a6989f82bda793abec7c36d643cdef3516`.
