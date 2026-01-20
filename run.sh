set -e

cd build

cmake ..

cmake --build . --parallel

if [ "$(uname)" = "Darwin" ]; then
  cd ./KICKSTART_artefacts/Standalone/KICKSTART.app/Contents/MacOS
fi

./KICKSTART