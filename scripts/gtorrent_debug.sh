# debug gTorrent using valgrind

function gtorrent_debug() {
	local options="--tool=memcheck --leak-check=yes"
	valgrind $options --log-file=valgrind.log ../gTorrent $1
}

gtorrent_debug $1
