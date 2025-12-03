# 测试指南

## 测试实现说明

### 测试1：时钟滴答测试
- **实现位置**：`kernel/dev/timer.c` 的 `timer_on_tick()` 函数
- **功能**：每个 tick 输出一个 'T' 字符
- **代码**：第58行 `uart_putc_sync('T');`

### 测试2：时钟快慢测试
- **实现位置**：`kernel/dev/timer.c` 的 `timer_on_tick()` 函数
- **功能**：每100个tick输出一次ticks值
- **代码**：第60-62行
- **修改间隔**：修改 `TICK_INTERVAL` 宏定义（第12行）来观察时钟快慢
  - 当前值：`100000UL` (约0.01秒/tick)
  - 增大值：时钟变慢（如 `200000UL`）
  - 减小值：时钟变快（如 `50000UL`）

### 测试3：UART输入响应测试
- **实现位置**：`kernel/trap/trap_kernel.c` 的 `trap_kernel_handler()` 函数
- **功能**：键盘输入的字符会被回显到屏幕上
- **代码**：第40-41行处理UART中断并调用 `uart_intr()` 回显字符

## 运行测试

1. **编译项目**：
   ```bash
   make clean
   make
   ```

2. **运行QEMU**：
   ```bash
   make qemu
   ```

3. **观察测试结果**：
   - 测试1：应该看到连续的 'T' 字符输出
   - 测试2：每100个tick会看到 `[ticks=100]`、`[ticks=200]` 等输出
   - 测试3：在QEMU窗口中输入字符，应该看到字符被回显

4. **修改时钟间隔测试**：
   - 编辑 `kernel/dev/timer.c` 第12行的 `TICK_INTERVAL` 值
   - 重新编译并运行，观察 'T' 字符输出速度的变化

5. **退出QEMU**：
   - 按 `Ctrl+A` 然后按 `X` 退出QEMU

## 注意事项

- QEMU使用 `-nographic` 模式，所有输出都在终端中显示
- 确保已安装 `qemu-system-riscv64`
- 如果编译失败，检查是否所有依赖都已正确安装

