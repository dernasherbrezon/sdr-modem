#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include "../src/dsp/lpf.h"
#include "utils.h"

lpf *lpf_filter = NULL;
float *float_input = NULL;
float complex *complex_input = NULL;

START_TEST (test_complex) {
    int code = lpf_create(1, 48000, 4800, 2000, 2000, sizeof(float complex), &lpf_filter);
    ck_assert_int_eq(code, 0);
    
    setup_input_complex_data(&complex_input, 0, 500);
    float complex *output = NULL;
    size_t output_len = 0;
    lpf_process(complex_input, 250, (void**) &output, &output_len, lpf_filter);

    const float expected[] = {0.000000F,-0.000866F,-0.001732F,-0.003529F,-0.005327F,-0.007783F,-0.010240F,-0.012697F,-0.015153F,-0.016630F,-0.018106F,-0.017599F,-0.017091F,-0.014098F,-0.011105F,-0.006196F,-0.001287F,0.003622F,0.008531F,0.010528F,0.012525F,0.008788F,0.005052F,-0.005606F,-0.016263F,-0.032046F,-0.047829F,-0.063612F,-0.079394F,-0.087950F,-0.096505F,-0.091266F,-0.086026F,-0.064558F,-0.043089F,-0.009834F,0.023422F,0.056677F,0.089932F,0.106849F,0.123767F,0.109350F,0.094933F,0.043040F,-0.008853F,-0.088841F,-0.168829F,-0.248817F,-0.328806F,-0.364084F,-0.399362F,-0.336188F,-0.273014F,-0.059998F,0.153018F,0.552869F,0.952720F,1.552869F,2.153018F,2.940002F,3.726986F,4.663812F,5.600638F,6.635916F,7.671195F,8.751183F,9.831171F,10.911160F,11.991147F,13.043041F,14.094934F,15.109349F,16.123766F,17.106850F,18.089933F,19.056677F,20.023422F,20.990168F,21.956913F,22.935444F,23.913971F,24.908733F,25.903492F,26.912045F,27.920605F,28.936388F,29.952168F,30.967955F,31.983738F,32.994396F,34.005054F,35.008785F,36.012524F,37.010521F,38.008530F,39.003628F,39.998711F,40.993805F,41.988899F,42.985901F,43.982903F,44.982407F,45.981888F,46.983372F,47.984848F,48.987309F,49.989758F,50.992210F,51.994675F,52.996471F,53.998272F,54.999134F,56.000011F,57.000004F,58.000000F,59.000004F,60.000011F,61.000000F,62.000004F,63.000000F,64.000008F,65.000000F,66.000008F,66.999992F,67.999992F,69.000008F,70.000008F,71.000000F,72.000008F,73.000008F,74.000000F,74.999992F,76.000008F,76.999992F,77.999992F,78.999992F,79.999992F,81.000008F,82.000008F,83.000000F,83.999992F,85.000023F,86.000000F,87.000008F,87.999985F,89.000015F,89.999992F,91.000008F,92.000000F,93.000008F,94.000008F,95.000008F,96.000000F,97.000008F,98.000008F,99.000015F,100.000008F,101.000008F,102.000000F,103.000000F,104.000000F,105.000015F,106.000008F,107.000008F,108.000008F,109.000008F,110.000015F,110.999985F,112.000015F,113.000000F,114.000015F,115.000008F,116.000000F,116.999992F,118.000015F,119.000015F,120.000000F,121.000000F,121.999992F,123.000008F,124.000000F,125.000000F,126.000000F,127.000000F,127.999977F,129.000015F,130.000031F,131.000015F,131.999985F,133.000000F,134.000000F,135.000046F,136.000015F,137.000015F,138.000061F,139.000031F,139.999969F,140.999985F,142.000000F,143.000015F,143.999969F,145.000046F,146.000031F,147.000015F,147.999969F,149.000000F,150.000015F,150.999969F,152.000000F,153.000000F,154.000031F,155.000000F,156.000015F,156.999985F,158.000015F,159.000000F,160.000000F,161.000000F,162.000015F,162.999985F,164.000000F,164.999985F,165.999985F,166.999985F,167.999969F,169.000015F,170.000015F,171.000031F,172.000031F,173.000015F,174.000015F,175.000000F,176.000015F,176.999969F,178.000000F,179.000000F,180.000000F,181.000015F,182.000015F,183.000015F,184.000015F,185.000000F,186.000000F,186.999985F,188.000031F,189.000000F,190.000015F,191.000015F,192.000046F,193.000000F,194.000015F,194.999985F,196.000000F,197.000046F,197.999985F,199.000000F,200.000000F,201.000000F,202.000015F,203.000000F,204.000015F,205.000031F,206.000031F,206.999985F,208.000031F,209.000015F,209.999985F,211.000015F,211.999985F,213.000000F,213.999969F,214.999985F,216.000015F,217.000031F,218.000015F,219.000000F,220.000046F,221.000031F,222.000046F,223.000000F,224.000031F,225.000031F,225.999985F,227.000046F,228.000015F,229.000015F,229.999939F,231.000000F,232.000000F,233.000031F,234.000015F,235.000000F,236.000000F,237.000000F,238.000000F,239.000031F,240.000000F,241.000000F,242.000000F,243.000046F,243.999969F,245.000031F,246.000031F,246.999985F,248.000000F,248.999985F,249.999969F,251.000015F,252.000015F,253.000031F,254.000015F,255.000000F,256.000061F,257.000031F,257.999969F,259.000031F,260.000031F,261.000000F,262.000061F,262.999969F,263.999969F,264.999908F,265.999969F,266.999969F,267.999969F,268.999939F,269.999969F,271.000000F,272.000061F,273.000000F,273.999969F,274.999969F,275.999969F,276.999969F,278.000031F,279.000000F,280.000000F,280.999908F,282.000000F,283.000000F,284.000000F,285.000031F,286.000031F,286.999939F,288.000092F,289.000000F,289.999969F,291.000031F,291.999939F,293.000031F,294.000000F,294.999969F,296.000061F,296.999969F,298.000031F,299.000061F,300.000031F,301.000031F,302.000031F,303.000000F,304.000031F,304.999969F,306.000031F,306.999969F,308.000031F,309.000000F,309.999939F,311.000000F,312.000031F,313.000000F,314.000092F,315.000031F,315.999969F,317.000000F,318.000000F,319.000031F,320.000000F,320.999939F,322.000000F,322.999969F,324.000061F,325.000000F,325.999939F,327.000061F,328.000000F,329.000000F,329.999969F,331.000031F,332.000031F,333.000000F,334.000000F,335.000000F,336.000031F,337.000000F,337.999969F,338.999939F,340.000000F,341.000000F,342.000031F,343.000031F,344.000092F,345.000061F,346.000000F,347.000061F,348.000061F,349.000000F,350.000031F,351.000031F,352.000000F,353.000000F,353.999969F,354.999878F,355.999939F,356.999908F,358.000000F,359.000000F,360.000061F,361.000031F,362.000000F,362.999969F,364.000031F,364.999969F,366.000000F,366.999908F,368.000031F,369.000031F,370.000000F,371.000031F,371.999939F,372.999878F,374.000000F,374.999969F,375.999969F,377.000000F,378.000000F,379.000031F,380.000031F,380.999908F,382.000031F,382.999969F,384.000061F,384.999969F,385.999969F,387.000031F,388.000031F,389.000000F,389.999969F,390.999939F,392.000092F,393.000031F,394.000122F,395.000000F,396.000031F,397.000031F,397.999969F,398.999969F,400.000061F,401.000000F,401.999939F,402.999969F,403.999969F,404.999969F,405.999939F,406.999878F,408.000061F,408.999969F,410.000061F,411.000000F,412.000092F,413.000061F,414.000031F,415.000000F,416.000061F,417.000031F,418.000061F,418.999969F,420.000000F,420.999969F,421.999969F,423.000000F,423.999969F,425.000061F,426.000031F,426.999969F,428.000031F,429.000031F,430.000031F,430.999969F,432.000000F,433.000000F,434.000122F,435.000061F,435.999969F,436.999939F,438.000000F,439.000061F,440.000000F,440.999969F,441.999969F,443.000000F};
    assert_complex_array(expected, sizeof(expected) / sizeof(float) / 2, output, output_len);

    lpf_process(complex_input + 250, 250, (void**) &output, &output_len, lpf_filter);
    const float expected2[] = {444.000031F,444.999969F,446.000000F,447.000000F,447.999939F,449.000031F,450.000000F,450.999939F,452.000092F,452.999969F,454.000061F,454.999969F,455.999908F,457.000000F,458.000031F,459.000000F,460.000031F,460.999969F,462.000031F,463.000000F,464.000000F,464.999969F,465.999969F,466.999908F,467.999969F,468.999969F,470.000031F,471.000000F,472.000031F,473.000000F,473.999939F,474.999969F,475.999969F,477.000031F,478.000031F,479.000000F,479.999969F,480.999969F,482.000031F,483.000031F,483.999969F,485.000061F,486.000092F,487.000061F,488.000061F,489.000000F,490.000031F,491.000031F,491.999908F,493.000000F,494.000061F,495.000000F,496.000031F,496.999969F,497.999939F,498.999969F,500.000092F,501.000000F,502.000031F,502.999878F,504.000122F,504.999878F,506.000000F,506.999969F,507.999939F,509.000214F,510.000061F,511.000061F,511.999908F,513.000183F,514.000000F,514.999939F,515.999939F,516.999939F,517.999878F,518.999939F,519.999878F,521.000000F,521.999817F,523.000000F,524.000061F,525.000000F,526.000000F,526.999878F,527.999878F,529.000061F,530.000000F,531.000000F,532.000000F,532.999878F,534.000061F,535.000000F,535.999878F,536.999878F,538.000000F,539.000122F,540.000061F,541.000061F,542.000183F,543.000000F,544.000061F,545.000122F,546.000000F,547.000061F,548.000061F,548.999939F,550.000061F,551.000061F,552.000061F,552.999878F,554.000000F,555.000061F,556.000000F,557.000061F,558.000061F,559.000000F,560.000122F,561.000061F,562.000061F,562.999878F,564.000061F,564.999939F,566.000061F,567.000000F,568.000000F,568.999878F,570.000061F,571.000061F,571.999939F,573.000000F,574.000000F,575.000000F,576.000061F,577.000061F,578.000000F,579.000061F,579.999939F,580.999939F,582.000061F,583.000000F,583.999878F,584.999939F,586.000061F,587.000000F,588.000000F,589.000000F,589.999939F,590.999878F,592.000061F,593.000183F,594.000061F,595.000000F,596.000122F,597.000000F,598.000122F,599.000061F,600.000000F,600.999878F,602.000122F,603.000183F,604.000061F,604.999939F,605.999939F,606.999939F,607.999878F,609.000061F,609.999878F,611.000000F,611.999878F,612.999878F,614.000061F,614.999939F,615.999878F,617.000000F,618.000061F,618.999817F,620.000061F,621.000000F,622.000183F,623.000122F,624.000061F,625.000061F,626.000061F,627.000000F,628.000000F,629.000061F,630.000122F,630.999878F,631.999939F,633.000061F,634.000061F,635.000122F,635.999756F,637.000000F,637.999939F,639.000061F,640.000000F,640.999817F,641.999939F,643.000061F,644.000000F,645.000000F,645.999939F,646.999939F,647.999878F,648.999939F,650.000061F,651.000122F,651.999939F,653.000000F,654.000122F,655.000000F,655.999939F,657.000061F,657.999939F,658.999939F,660.000000F,661.000061F,662.000122F,663.000000F,664.000061F,665.000061F,666.000061F,667.000061F,668.000000F,669.000000F,670.000000F,671.000183F,672.000000F,673.000000F,673.999939F,675.000183F,676.000122F,677.000000F,678.000000F,679.000122F,680.000061F,681.000000F,682.000061F,683.000061F,683.999939F,685.000122F,686.000122F,687.000061F,688.000061F,689.000000F,690.000061F,691.000000F,692.000000F,692.999939F,694.000000F,695.000122F,695.999939F,696.999939F,698.000000F,698.999939F,699.999817F,701.000000F,702.000000F,703.000061F,704.000000F,705.000000F,705.999878F,707.000061F,707.999939F,708.999878F,709.999817F,711.000000F,712.000122F,713.000061F,714.000061F,714.999939F,716.000000F,717.000000F,717.999878F,719.000000F,719.999878F,721.000000F,722.000061F,723.000122F,724.000000F,724.999878F,725.999939F,726.999939F,727.999817F,729.000122F,730.000000F,731.000061F,732.000000F,733.000122F,734.000061F,735.000000F,736.000183F,737.000061F,738.000000F,739.000183F,739.999939F,741.000061F,742.000122F,743.000122F,744.000122F,744.999939F,746.000122F,747.000000F,748.000061F,748.999939F,750.000000F,751.000000F,752.000000F,752.999878F,754.000122F,755.000061F,755.999817F,757.000061F,758.000122F,759.000000F,760.000122F,760.999939F,762.000061F,763.000061F,764.000061F,765.000183F,766.000000F,767.000122F,768.000122F,769.000061F,770.000244F,771.000061F,772.000000F,773.000122F,774.000000F,775.000183F,776.000000F,777.000061F,778.000000F,779.000000F,780.000061F,780.999939F,781.999878F,782.999939F,784.000061F,785.000000F,786.000000F,786.999878F,787.999817F,788.999817F,789.999939F,791.000000F,791.999939F,792.999939F,794.000061F,795.000122F,796.000122F,797.000000F,798.000000F,799.000000F,800.000000F,801.000061F,802.000000F,803.000000F,803.999939F,805.000061F,806.000122F,807.000000F,808.000061F,808.999817F,810.000061F,810.999878F,812.000061F,813.000000F,814.000000F,815.000061F,816.000122F,816.999878F,818.000000F,818.999939F,820.000000F,821.000000F,822.000000F,823.000061F,824.000000F,825.000061F,826.000122F,826.999939F,827.999878F,829.000000F,829.999939F,831.000183F,832.000000F,833.000122F,834.000000F,835.000122F,836.000061F,837.000000F,838.000061F,839.000000F,840.000000F,841.000000F,842.000244F,843.000000F,843.999939F,845.000000F,845.999939F,847.000061F,848.000000F,849.000000F,849.999939F,851.000061F,852.000000F,853.000000F,854.000061F,855.000061F,855.999939F,857.000061F,857.999939F,859.000000F,859.999939F,860.999939F,862.000000F,863.000000F,864.000061F,865.000061F,866.000061F,867.000305F,868.000061F,869.000183F,870.000183F,871.000061F,872.000000F,873.000061F,874.000122F,874.999939F,876.000061F,877.000061F,877.999939F,879.000061F,879.999939F,880.999878F,882.000061F,883.000061F,884.000061F,885.000244F,886.000000F,887.000122F,887.999939F,889.000061F,889.999939F,890.999878F,891.999878F,892.999878F,893.999939F,895.000122F,895.999878F,896.999939F,898.000000F,899.000000F,899.999939F,900.999939F,902.000000F,903.000122F,904.000061F,905.000183F,906.000000F,907.000122F,908.000000F,908.999817F,910.000061F,910.999939F,911.999817F,913.000000F,914.000000F,915.000061F,915.999939F,917.000000F,918.000061F,919.000000F,920.000000F,921.000122F,921.999878F,923.000000F,923.999878F,925.000061F,925.999878F,926.999939F,927.999756F,928.999817F,930.000122F,931.000000F,932.000061F,933.000061F,934.000122F,935.000305F,936.000183F,937.000122F,938.000183F,939.000122F,940.000061F,940.999939F,942.000183F,943.000122F};
    assert_complex_array(expected2, sizeof(expected2) / sizeof(float) / 2, output, output_len);
}
END_TEST

START_TEST (test_normal) {
	int code = lpf_create(2, 48000, 4800, 2000, 2000, sizeof(float), &lpf_filter);
	ck_assert_int_eq(code, 0);

	setup_input_data(&float_input, 0, 1000);
	float *output = NULL;
	size_t output_len = 0;
	lpf_process(float_input, 500, (void**) &output, &output_len, lpf_filter);

	const float expected[] = { 0.000000F,-0.002663F,-0.007577F,-0.008546F,-0.000644F,0.006263F,-0.008132F,-0.039697F,-0.043013F,0.011711F,0.061883F,-0.004426F,-0.164403F,-0.136507F,0.476360F,1.863493F,3.835597F,5.995574F,8.061883F,10.011711F,11.956985F,13.960302F,15.991869F,18.006262F,19.999355F,21.991451F,23.992424F,25.997337F,28.000006F,30.000006F,32.000004F,33.999996F,36.000004F,38.000004F,39.999996F,41.999996F,43.999992F,46.000000F,48.000000F,50.000004F,52.000000F,54.000004F,56.000008F,58.000000F,60.000000F,62.000000F,63.999989F,65.999992F,68.000008F,69.999985F,71.999985F,73.999985F,76.000000F,78.000008F,80.000000F,82.000000F,83.999985F,86.000015F,88.000008F,90.000000F,92.000008F,94.000015F,96.000023F,98.000000F,100.000000F,102.000008F,104.000015F,105.999992F,108.000008F,110.000023F,112.000015F,114.000008F,116.000000F,118.000000F,120.000000F,121.999985F,124.000000F,126.000008F,128.000031F,130.000015F,131.999985F,133.999985F,136.000031F,137.999985F,140.000000F,142.000000F,144.000046F,145.999969F,148.000031F,150.000015F,152.000015F,154.000015F,156.000015F,157.999985F,160.000000F,162.000031F,164.000000F,166.000015F,168.000015F,170.000000F,172.000046F,174.000031F,176.000000F,177.999969F,180.000031F,182.000015F,184.000015F,185.999969F,187.999985F,190.000015F,192.000031F,194.000015F,196.000046F,198.000015F,200.000031F,201.999985F,204.000031F,206.000046F,208.000031F,210.000000F,211.999985F,214.000015F,216.000000F,217.999985F,220.000000F,222.000015F,223.999969F,226.000046F,227.999954F,230.000015F,232.000000F,233.999985F,236.000015F,237.999985F,239.999985F,241.999985F,244.000031F,245.999954F,248.000015F,250.000046F,252.000061F,253.999969F,255.999954F,257.999969F,259.999939F,262.000031F,263.999939F,266.000000F,267.999939F,270.000031F,272.000031F,274.000031F,276.000031F,278.000000F,280.000061F,282.000031F,284.000000F,285.999969F,288.000031F,289.999969F,291.999939F,294.000000F,296.000031F,298.000061F,300.000000F,302.000031F,303.999939F,305.999939F,307.999939F,310.000031F,312.000031F,314.000000F,315.999969F,317.999878F,320.000000F,322.000000F,323.999939F,325.999969F,327.999969F,330.000000F,332.000031F,334.000000F,336.000000F,338.000061F,340.000031F,341.999969F,344.000031F,346.000000F,347.999969F,349.999908F,352.000000F,353.999969F,356.000061F,358.000000F,359.999939F,362.000000F,363.999908F,366.000000F,368.000092F,369.999969F,372.000061F,374.000031F,376.000000F,377.999908F,380.000061F,382.000031F,384.000061F,386.000000F,388.000000F,390.000031F,392.000031F,393.999908F,395.999969F,398.000061F,400.000000F,401.999969F,404.000031F,406.000031F,408.000061F,410.000000F,412.000000F,413.999939F,416.000000F,418.000031F,420.000000F,421.999969F,424.000000F,426.000000F,427.999969F,429.999969F,432.000031F,434.000031F,436.000000F,438.000031F,439.999969F,442.000031F,443.999969F,445.999939F,447.999939F,449.999969F,452.000031F,454.000000F,455.999908F,457.999969F,460.000000F,461.999939F,463.999878F,466.000031F,468.000092F,470.000031F };
	assert_float_array(expected, sizeof(expected) / sizeof(float), output, output_len);

	lpf_process(float_input + 500, 500, (void**) &output, &output_len, lpf_filter);
    const float expected2[] = {471.999969F,473.999969F,476.000061F,478.000092F,479.999969F,481.999969F,484.000061F,485.999969F,488.000061F,489.999939F,492.000000F,494.000031F,495.999969F,498.000000F,499.999908F,502.000092F,504.000092F,506.000061F,507.999878F,510.000031F,511.999878F,514.000061F,516.000061F,518.000000F,519.999878F,522.000061F,524.000122F,526.000000F,528.000000F,530.000061F,532.000122F,534.000061F,536.000061F,537.999878F,540.000000F,542.000122F,543.999939F,546.000000F,548.000000F,550.000000F,552.000000F,553.999878F,555.999939F,557.999939F,560.000061F,561.999939F,564.000122F,566.000061F,568.000000F,570.000122F,571.999939F,574.000061F,576.000000F,577.999878F,580.000000F,582.000061F,584.000061F,586.000000F,587.999939F,590.000000F,592.000122F,593.999939F,596.000061F,598.000000F,600.000061F,602.000000F,603.999878F,605.999939F,608.000000F,609.999878F,612.000061F,614.000000F,616.000061F,618.000244F,620.000061F,622.000122F,623.999939F,625.999939F,628.000122F,630.000000F,632.000061F,633.999939F,636.000000F,638.000061F,640.000000F,642.000000F,643.999878F,646.000122F,648.000061F,650.000122F,652.000061F,654.000122F,656.000061F,657.999939F,660.000000F,662.000122F,664.000183F,666.000000F,667.999939F,670.000061F,672.000061F,674.000000F,676.000122F,678.000061F,680.000183F,682.000122F,684.000000F,686.000000F,687.999939F,690.000061F,692.000000F,693.999878F,695.999878F,697.999878F,700.000000F,701.999939F,703.999939F,705.999878F,707.999939F,710.000061F,712.000122F,714.000000F,716.000000F,718.000061F,720.000122F,722.000000F,724.000061F,725.999817F,728.000122F,730.000061F,732.000061F,734.000000F,736.000000F,737.999939F,740.000061F,742.000061F,744.000061F,746.000122F,748.000122F,749.999878F,751.999939F,753.999878F,756.000000F,758.000061F,759.999878F,762.000061F,764.000061F,766.000000F,768.000061F,770.000000F,772.000000F,774.000000F,776.000061F,778.000000F,780.000061F,782.000000F,784.000183F,785.999939F,787.999939F,789.999878F,791.999939F,794.000122F,795.999939F,797.999878F,799.999939F,802.000061F,804.000000F,805.999817F,807.999939F,810.000122F,812.000183F,814.000122F,816.000122F,818.000000F,820.000305F,822.000000F,824.000000F,826.000000F,828.000000F,830.000061F,832.000061F,834.000061F,836.000061F,838.000000F,840.000000F,841.999939F,844.000061F,846.000061F,848.000122F,850.000061F,852.000061F,854.000122F,856.000000F,858.000122F,859.999939F,862.000061F,864.000122F,866.000000F,868.000061F,870.000000F,872.000061F,874.000122F,876.000000F,877.999878F,879.999939F,882.000061F,884.000061F,886.000061F,887.999939F,889.999939F,892.000000F,893.999939F,895.999878F,898.000061F,899.999878F,902.000000F,903.999939F,906.000000F,907.999939F,909.999878F,912.000000F,914.000122F,916.000000F,918.000000F,920.000122F,921.999878F,924.000122F,926.000061F,928.000061F,930.000061F,932.000000F,934.000061F,936.000000F,938.000000F,940.000122F,942.000000F,944.000122F,946.000000F,947.999939F,950.000061F,952.000122F,954.000000F,956.000000F,957.999878F,959.999878F,961.999939F,964.000061F,966.000183F,968.000000F,969.999878F};
    assert_float_array(expected2, sizeof(expected2) / sizeof(float), output, output_len);
}
END_TEST

void teardown() {
	if (lpf_filter != NULL) {
		lpf_destroy(lpf_filter);
        lpf_filter = NULL;
	}
	if( float_input != NULL ) {
		free(float_input);
        float_input = NULL;
	}
    if( complex_input != NULL ) {
        free(complex_input);
        complex_input = NULL;
    }
}

void setup() {
	//do nothing
}

Suite* common_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("lpf");

	/* Core test case */
	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_normal);
    tcase_add_test(tc_core, test_complex);

	tcase_add_checked_fixture(tc_core, setup, teardown);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(void) {
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = common_suite();
	sr = srunner_create(s);

	srunner_set_fork_status(sr, CK_NOFORK);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
