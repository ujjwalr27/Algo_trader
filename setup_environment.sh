
sudo apt update

sudo apt install -y build-essential cmake

sudo apt install -y libboost-all-dev libssl-dev


if [ ! -d "$HOME/vcpkg" ]; then
  git clone https://github.com/microsoft/vcpkg.git $HOME/vcpkg
  $HOME/vcpkg/bootstrap-vcpkg.sh
fi

Install nlohmann/json via vcpkg
$HOME/vcpkg/vcpkg install nlohmann-json

# Export vcpkg environment variables to CMake
echo "Set VCPKG to automatically integrate with CMake:"
export VCPKG_ROOT=$HOME/vcpkg
echo "export VCPKG_ROOT=\$HOME/vcpkg" >> ~/.bashrc
source ~/.bashrc

echo "Environment setup complete. You can now run CMake to build your project."
