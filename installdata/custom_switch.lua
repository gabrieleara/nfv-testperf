-- Load all packages
module(..., package.seeall)

-- Include modules
local config    = require("core.config")
local virtio    = require("apps.vhost.vhost_user")
local learning  = require("apps.bridge.learning")

function print_usage()
    print("Usage: " .. "custom_switch" .. " <num_vports> [<vport_name>...] [<ether_pci>]")
    print("A learning bridge is set in the middle among all the ports")
end

-- Requires first argument to be the number of virtual ports to instantiate
-- Then follows the names of each virtual port
-- Finally, an optional last argument with the PCI address of a DPDK physical
-- device can be provided [FIXME: UNUSED FOR NOW]
function run (parameters)
    -- Checking Parameters
    if not (#parameters > 0) then
        print_usage()
        main.exit(1)
    end

    local num_vports = tonumber(parameters[1])
    local port_names = {}
    local port_paths = {}
    local eport_use = false
    local eport_pci = ''
    local num_ports = num_vports

    print("NUM_VPORTS:" .. num_vports)

    if (not (num_vports)) or num_vports < 1 then
        print("First argument must be a nonzero number!")
        print_usage()
        main.exit(1)
    end

    if not ((#parameters) >= (num_vports + 1)) then
        print("Not enough arguments!")
        print("The first number must be the number of virtual ports to create!")
        print_usage()
        main.exit(1)
    end

    -- virtual ports are from index 2 (1+1) to index num_vports+1 (included)
    -- optinally, index num_vports+2 contains the pci address of the ethernet
    -- port

    -- Copy virtual port names in an array (1-indexed)
    for i=1,num_vports do
        port_paths[i] = parameters[i+1]
        port_names[i] = "sock" .. i
    end

    -- If a PCI address parameter is provided, add it to the port list
    if (#parameters) >= (num_vports + 2) then
        eport_use = true
        eport_pci = parameters[num_vports+2]
        num_ports = num_vports+1
        port_names[num_ports] = "sriov"
    end

    -- Configuration structure
    local c = config.new()

    -- Configure the learning virtual switch
    config.app(c, "bridge", learning.bridge, {ports=port_names})

    -- Configure virtio apps
    for i=1,num_vports do
        config.app(c, port_names[i] , virtio.VhostUser, {socket_path=port_paths[i], is_server=true})

        -- Configure a bi-directional link between each virtual port and the
        -- bridge
        local vport_tx = port_names[i] .. ".tx"
        local vport_rx = port_names[i] .. ".rx"
        local bridge_port = "bridge." .. port_names[i]

        config.link(c, vport_tx     .. " -> " .. bridge_port)
        config.link(c, bridge_port  .. " -> " .. vport_rx)
    end

    if eport_use then
        -- TODO: configure PCI device and attach to bridge port
    end

    -- Apply the configuration for the system
    engine.configure(c)

    -- Start the configured application
    engine.main({report = {showlinks=true, showapps=true}})
--  engine.main({report = {showlinks=true}}) -- engine.busywait=true
end
