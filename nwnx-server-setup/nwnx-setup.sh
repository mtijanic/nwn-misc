# NOTE: This is not a runnable file - you need to manually paste the lines one by one
# Take some time to understand what each command does.
# These steps were tested on a clean Ubuntu 18.04 Desktop install:

#
# Install necessary prereqs
#
# We use a new compiler which may not be available by default, so add an extra place to download packages from
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
# Redownload manifests for the newly added architecture and repository so we can use them
sudo apt update
# Upgrade any existing packages to the newest versions
sudo apt upgrade
# Install tools needed to build NWNX
sudo apt install g++-7 g++-7-multilib gcc-7 gcc-7-multilib cmake git make unzip
# Install stuff needed to build/run/use MySQL
sudo apt install mysql-server  libmysqlclient20 libmysqlclient-dev

#
# Download and build NWNX
#
# Get latest source from github
git clone https://github.com/nwnxee/unified.git nwnx
# Make a directory where the build system will initialize
mkdir nwnx/build && cd nwnx/build
# Initialize the build system to use GCC version 7. Build release version of nwnx, with debug info
CC=gcc-7 CXX=g++-7 cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ../
# Build NWNX, in 6 threads. This will take a while
make -j6

#
# Download NWN dedicated package
#
# Make a directory to hold NWN data
mkdir ~/nwn && cd ~/nwn
# Fetch the NWN dedicated server package. The version here might be outdated, so replace 8193.10 with current NWN build version
wget https://nwnx.io/nwnee-dedicated-8193.10.zip
# Unpack the server to current directory - ~/nwn
unzip nwnee-dedicated-8193.10.zip -d .

# Run it once to create the user directory.
# nwserver must be run with current directory the same as the executable, so we need to `cd` into it first
cd bin/linux-x86 && ./nwserver-linux
# The user directory path is long and contains spaces, which is hard to type sometimes.
# So we create a link (shortcut) to it as ~/nwn/userdir so it's easier to access
ln -s ~/.local/share/Neverwinter\ Nights/ ~/nwn/userdir

# Set up your module:
# Copy your module/hak/tlk/etc files to ~/nwn/userdir
# Edit ~/nwn/userdir/nwnplayer.ini to your preference

#
# Set up version control on the servervault
# This is useful so you can restore player character backups if something goes wrong at any time
#
cd ~/nwn/userdir/servervault
# We'll use git for version control since that's what NWNX uses.
git init
git config --global user.name = "My Name"
git config --global user.email = "my@email.com"

#
# Set up the Database
#
# Run mysql (as admin/sudo). The following commands are given in MySQL, not the regular terminal
sudo mysql
# We want to configure mysql itself.
mysql> USE mysql;
# Create a new user for nwserver to use. 'nwn' is username, 'pass' is password, you can change them if you want.
mysql> CREATE USER 'nwn'@'localhost' IDENTIFIED BY 'pass';
# Give it full access
mysql> GRANT ALL PRIVILEGES ON *.* TO 'nwn'@'localhost';
mysql> FLUSH PRIVILEGES;
# Create a database for the module to use, typically named same as module, but can be anything.
mysql> CREATE DATABASE mymodulename;
mysql> exit;

#
# Copy the scripts from this directory over onto ~/
# mod-start.sh - starts the server unless already running
# mod-stop.sh - kills the server
# mod-disable.sh - disables server auto restart
# mod-enable.sh - enables server auto restart
# mod-savechars.sh - saves servervault/ to git
# mod-status.sh - returns 1 if server is running, 0 if not

# Need to mark them as executable
chmod +x mod-*.sh

# Create a place where logs will be stored
mkdir ~/logs

# Edit the mod-start.sh script to further customize. You can use any other text editor instead.
nano ~/mod-start.sh


#
# Set up cronjob for auto server restart every minute
#
# Cron lets you run some script at specified intervals. You need to run this command, and then add the line exactly as given below
crontab -e
    # Add this line to the tab
    */1 * * * * ~/mod-start.sh

#
# Start the server. If it goes down, the cron job will restart it again within 1 minute.
#
./mod-start.sh
