library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity fsm_test is
    Port ( 
        aclk          : in  STD_LOGIC;
        aresetn       : in  STD_LOGIC;  
        start         : in  STD_LOGIC;
        
        -- AXI-Stream Ports
        m_axis_tdata  : out STD_LOGIC_VECTOR(31 downto 0);
        m_axis_tvalid : out STD_LOGIC;
        m_axis_tlast  : out STD_LOGIC;
        m_axis_tready : in  STD_LOGIC; -- Best practice: check if receiver is ready
        
        -- Debug Port for ILA
        current_state_out : out STD_LOGIC_VECTOR(1 downto 0)
    );
end fsm_test;

architecture Behavioral of fsm_test is
    type state_type is (IDLE, WORKING, COMPLETED); 
    signal state : state_type := IDLE;
begin

    -- Mapping state to a vector for the ILA (00=IDLE, 01=WORKING, 10=COMPLETED)
    current_state_out <= "00" when state = IDLE else
                         "01" when state = WORKING else
                         "10";

    process(aclk)
    begin
        if rising_edge(aclk) then
            if aresetn = '0' then
                state <= IDLE;
                m_axis_tvalid <= '0';
                m_axis_tlast <= '0';
                m_axis_tdata <= (others => '0');
            else
                case state is
                    when IDLE =>
                        m_axis_tvalid <= '0';
                        m_axis_tlast <= '0';
                        if start = '1' then
                            state <= WORKING;
                        end if;

                    when WORKING =>
                        -- Capture data here 
                        m_axis_tdata <= x"DEADBEEF"; 
                        m_axis_tvalid <= '1';
                        state <= COMPLETED;

                    when COMPLETED =>
                        m_axis_tvalid <= '1';
                        m_axis_tlast <= '1'; -- Signal the end of the DMA packet
                        
                        -- Wait for start to go low before allowing a new cycle
                        if start = '0' then
                            state <= IDLE;
                            --m_axis_tvalid <= '0';
                            --m_axis_tlast <= '0';
                        end if;
                end case;
            end if;
        end if;
    end process;

end Behavioral;
