library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity tb_fsm_test_axi is
end tb_fsm_test_axi;

architecture Behavioral of tb_fsm_test_axi is
    -- Signals to connect to UUT
    signal aclk          : std_logic := '0';
    signal aresetn       : std_logic := '0';
    signal start         : std_logic := '0';
    signal m_axis_tdata  : std_logic_vector(31 downto 0);
    signal m_axis_tvalid : std_logic;
    signal m_axis_tlast  : std_logic;
    signal m_axis_tready : std_logic := '0';
    signal state_debug   : std_logic_vector(1 downto 0);

    -- Clock constant
    constant CLK_PERIOD : time := 10 ns; -- 100 MHz

begin
    -- Instantiate the Unit Under Test (UUT)
    uut: entity work.fsm_test
        port map (
            aclk              => aclk,
            aresetn           => aresetn,
            start             => start,
            m_axis_tdata      => m_axis_tdata,
            m_axis_tvalid     => m_axis_tvalid,
            m_axis_tlast      => m_axis_tlast,
            m_axis_tready     => m_axis_tready,
            current_state_out => state_debug
        );

    -- Clock Generation
    clk_process : process
    begin
        aclk <= '0';
        wait for CLK_PERIOD/2;
        aclk <= '1';
        wait for CLK_PERIOD/2;
    end process;

    -- Stimulus Process
    stim_proc: process
    begin
        -- 1. Initial State: Reset Active
        aresetn <= '0';
        start   <= '0';
        m_axis_tready <= '1'; -- Assume FIFO is ready for now
        wait for CLK_PERIOD * 5;

        -- 2. Release Reset
        aresetn <= '1';
        wait for CLK_PERIOD * 2;

        -- 3. First SPI Acquisition Cycle
        wait until falling_edge(aclk);
        start <= '1';
        -- Observe WORKING state then COMPLETED state
        wait for CLK_PERIOD * 3;
        
        -- End of cycle: release start
        start <= '0';
        wait for CLK_PERIOD * 5;

        -- 4. Test Backpressure (Receiver not ready)
        -- Even if tready is low, your current FSM will pulse valid.
        -- In future SPI versions, you'd use tready to "pause" the FSM.
        m_axis_tready <= '0';
        wait until falling_edge(aclk);
        start <= '1';
        wait for CLK_PERIOD * 2;
        start <= '0';

        wait for 100 ns;
        report "AXI FSM Simulation Complete";
        wait;
    end process;

end Behavioral;
