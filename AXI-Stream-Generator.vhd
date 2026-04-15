library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity axis_simple_gen is
    generic (
        DATA_WIDTH : integer := 64
    );
    port (
        aclk          : in  std_logic;
        aresetn       : in  std_logic;
        -- AXI-Stream Master Interface
        m_axis_tvalid : out std_logic;
        m_axis_tready : in  std_logic;
        m_axis_tdata  : out std_logic_vector(DATA_WIDTH-1 downto 0);
        m_axis_tkeep  : out std_logic_vector((DATA_WIDTH/8)-1 downto 0);
        m_axis_tlast  : out std_logic;
        m_axis_tuser  : out std_logic_vector(0 downto 0)
    );
end entity axis_simple_gen;

architecture rtl of axis_simple_gen is
    -- Internal signals
    signal counter    : unsigned(31 downto 0) := (others => '0');
    signal tvalid_reg : std_logic := '0';
    
    -- Constants for readability
    constant MAX_COUNT : unsigned(31 downto 0) := to_unsigned(499, 32);
    constant HEADER    : std_logic_vector(31 downto 0) := x"DEADBEEF";
begin

    -- AXI-Stream Control Logic
    process(aclk)
    begin
        if rising_edge(aclk) then
            if aresetn = '0' then
                counter    <= (others => '0');
                tvalid_reg <= '0';
            else
                tvalid_reg <= '1'; -- Always have data ready
                
                -- Handshake occurs when Master is valid and Slave is ready
                if (tvalid_reg = '1' and m_axis_tready = '1') then
                    if counter = MAX_COUNT then
                        counter <= (others => '0');
                    else
                        counter <= counter + 1;
                    end if;
                end if;
            end if;
        end if;
    end process;

    -- Data assignments
    m_axis_tvalid <= tvalid_reg;
    
    -- Concatenating the header and the counter
    -- m_axis_tdata(63 downto 32) = DEADBEEF, (31 downto 0) = counter
    m_axis_tdata <= HEADER & std_logic_vector(counter);
    
    -- All bytes are valid (8 bits of '1' for a 64-bit bus)
    m_axis_tkeep <= (others => '1');
    
    -- TLAST asserted on the last beat (499)
    m_axis_tlast <= '1' when (counter = MAX_COUNT) else '0';
    
    m_axis_tuser <= "0";

end architecture rtl;
