package org.example;

import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.BufferedReader;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.logging.Logger;



import com.ibm.wala.classLoader.Module;
import magpiebridge.core.AnalysisConsumer;
import magpiebridge.core.AnalysisResult;
import magpiebridge.core.MagpieServer;
import magpiebridge.core.ToolAnalysis;
import org.eclipse.lsp4j.MessageParams;
import org.eclipse.lsp4j.MessageType;
/**
 * Wukong工具分析类，实现ToolAnalysis接口，用于执行Wukong命令行工具并处理其输出结果
 */
public class WukongToolAnalysis implements ToolAnalysis {
    private static final Logger LOG = Logger.getLogger("WukongToolAnalysis");
    private MagpieServer server;
    private Collection<AnalysisResult> results = Collections.emptyList();
    private String classPath = "/app/tests/qltests/java/adven";
    // 添加错误处理方法
    private void handleError(MagpieServer server, Exception e) {
        handleError(server, e.getLocalizedMessage());
    }

    private void handleError(MagpieServer server, String message) {
        if (server != null) {
            server.forwardMessageToClient(new MessageParams(MessageType.Error, message));
        }
        LOG.severe(message);
    }

    private int sourceId = 123; // 默认源ID
    private int sinkId = 456;   // 默认汇ID
    
    /**
     * 构造函数
     */
    public WukongToolAnalysis() {
        LOG.info("WukongToolAnalysis 初始化");
    }
    

    public void setClassPath(String classPath) {
        this.classPath = classPath;
        LOG.info("设置classpath: " + classPath);
    }

    /**
     * 设置分析的源ID和汇ID
     * @param sourceId 源ID
     * @param sinkId 汇ID
     */
    public void setSourceAndSink(int sourceId, int sinkId) {
        this.sourceId = sourceId;
        this.sinkId = sinkId;
        LOG.info("设置分析参数: sourceId=" + sourceId + ", sinkId=" + sinkId);
    }

    @Override
    public String source() {
        return "Wukong";
    }

    @Override
    public String[] getCommand() {
        // 为Windows环境准备命令
        List<String> cmdList = new ArrayList<>();
        
        // // 使用cmd.exe而不是bash
        // cmdList.add("cmd.exe");
        // cmdList.add("/c");
        // cmdList.add("echo 执行Wukong分析: sourceId=" + sourceId + ", sinkId=" + sinkId);
        
        // WSL环境下直接使用bash命令
        cmdList.add("bash");
        cmdList.add("-c");
        
        // 构建完整的gradle命令字符串
        // 先检查目录和gradlew是否存在，再执行命令
        // String gradleCmd = "if [ -d \"/mnt/d/Course/Year4/BS/WALA-start\" ] && [ -x \"/mnt/d/Course/Year4/BS/WALA-start/gradlew\" ]; then " +
        // "cd /mnt/d/Course/Year4/BS/WALA-start && " +
        // "./gradlew run --args=\"-classpath /app/tests/qltests/java/advennet/\"; " +
        // "else echo \"Error: WALA-start directory or gradlew not found\"; exit 1; fi";
        String gradleCmd = "if [ -d \"/mnt/d/Course/Year4/BS/WALA-start\" ] && [ -x \"/mnt/d/Course/Year4/BS/WALA-start/gradlew\" ]; then " +
        "cd /mnt/d/Course/Year4/BS/WALA-start && " +
        "./gradlew run --args=\"-classpath " + classPath + "\"; " +
        "else echo \"Error: WALA-start directory or gradlew not found\"; exit 1; fi";
        cmdList.add(gradleCmd);
        
        LOG.info("准备执行gradle命令: " + String.join(" ", cmdList));
        return cmdList.toArray(new String[0]);

        // 实际的Wukong命令可能类似于：
        // cmdList.add("java");
        // cmdList.add("-Xmx16g");
        // cmdList.add("-jar");
        // cmdList.add("d:\\path\\to\\wukongJava.jar");
        // cmdList.add("-S");
        // cmdList.add("d:\\temp\\AdventNetPassTrix_" + sourceId + "_" + sinkId + ".json");
        // cmdList.add("-target");
        // cmdList.add("d:\\temp\\JAR");
        // cmdList.add("-OD");
        // cmdList.add("d:\\temp\\output\\wukongBugs_" + sourceId + "_" + sinkId);
        
        // LOG.info("准备执行命令: " + String.join(" ", cmdList));
        // return cmdList.toArray(new String[0]);
    }

    @Override
    public Collection<AnalysisResult> convertToolOutput() {
        // 这里应该实现将Wukong工具的输出转换为AnalysisResult集合
        LOG.info("转换Wukong分析结果");
        return results;
    }

    @Override
    public void analyze(Collection<? extends Module> files, AnalysisConsumer consumer, boolean rerun) {
        LOG.info("开始Wukong分析，文件数量: " + files.size());
        System.out.println("开始Wukong分析，文件数量: " + files.size());
        
        // 保存consumer引用，通常是MagpieServer
        if (consumer instanceof MagpieServer) {
            this.server = (MagpieServer) consumer;
        }
        
        // 获取工作目录
        File workDir = new File(System.getProperty("user.dir"));
        LOG.info("工作目录: " + workDir.getAbsolutePath());
        System.out.println("工作目录: " + workDir.getAbsolutePath());
        
        try {
            // 执行命令
            LOG.info("执行Wukong命令...");
            System.out.println("执行Wukong命令...");
            
            // 使用ToolAnalysis接口提供的runCommand方法执行命令
            Process process = runCommand(workDir);
            
            // 等待命令执行完成
            int exitCode = process.waitFor();
            BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String line;
            while ((line = reader.readLine()) != null) {
                System.out.println("命令输出: " + line);
            }
            LOG.info("Wukong命令执行完成，退出码: " + exitCode);
            System.out.println("Wukong命令执行完成，退出码: " + exitCode);
            
            // 如果需要，可以读取命令的输出
            // Stream<String> output = runCommandAndReturnOutput(workDir);
            // output.forEach(line -> System.out.println("命令输出: " + line));
            
            // 转换结果并发送给consumer
            results = convertToolOutput();
            LOG.info("分析完成，结果数量: " + results.size());
            System.out.println("分析完成，结果数量: " + results.size());
            
            // 将结果发送给consumer
            consumer.consume(results, source());
            LOG.info("结果已发送给客户端");
            System.out.println("结果已发送给客户端");
            
        } catch (IOException e) {
            handleError(server, e);
        } catch (InterruptedException e) {
            handleError(server, e);
            Thread.currentThread().interrupt();
        }
    }
    
}