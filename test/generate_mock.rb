$LOAD_PATH.unshift(File.expand_path(File.join(ARGV[0], 'lib')))

require 'cmock'

cmock_root = ARGV[0]
config_file = ARGV[1]
header_file = ARGV[2]

abort('Usage: generate_mock.rb <cmock_root> <config_file> <header_file>') unless cmock_root && config_file && header_file

cmock = CMock.new(config_file)
cmock.setup_mocks([header_file])
