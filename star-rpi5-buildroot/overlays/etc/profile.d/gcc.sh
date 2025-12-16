# ARM GNU Toolchain 14.2 (native aarch64)
export GCC_HOME=/usr/local/gcc
export PATH=$GCC_HOME/bin:$PATH

# Create convenience aliases for common tools
alias gcc='aarch64-none-linux-gnu-gcc'
alias g++='aarch64-none-linux-gnu-g++'
alias cc='aarch64-none-linux-gnu-gcc'
alias c++='aarch64-none-linux-gnu-g++'
alias cpp='aarch64-none-linux-gnu-cpp'
alias ar='aarch64-none-linux-gnu-ar'
alias as='aarch64-none-linux-gnu-as'
alias ld='aarch64-none-linux-gnu-ld'
alias nm='aarch64-none-linux-gnu-nm'
alias objcopy='aarch64-none-linux-gnu-objcopy'
alias objdump='aarch64-none-linux-gnu-objdump'
alias ranlib='aarch64-none-linux-gnu-ranlib'
alias readelf='aarch64-none-linux-gnu-readelf'
alias size='aarch64-none-linux-gnu-size'
alias strings='aarch64-none-linux-gnu-strings'
alias strip='aarch64-none-linux-gnu-strip'
