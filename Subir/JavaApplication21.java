package javaapplication21;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.Writer;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class JavaApplication21 {

    public static void main(String[] args) throws Exception {

        Map<String, Integer> mapPalavras;
        mapPalavras = new HashMap<String, Integer>();

        FileReader txtFile = new FileReader(args[0]);

        BufferedReader txtBuffer = new BufferedReader(txtFile);

        String curLine = txtBuffer.readLine();

        Writer bw = new BufferedWriter(new FileWriter(args[1], true));

        while (curLine != null) {

            String minusculo = curLine.toLowerCase();

            Pattern p = Pattern.compile("(\\d+)|([a-záéíóúçãõôê]+)");

            Matcher m = p.matcher(minusculo);

            while (m.find()) {

                String token = m.group();
                Integer freq = mapPalavras.get(token);

                if (freq != null) {
                    mapPalavras.put(token, freq + 1);
                } else {

                    mapPalavras.put(token, 1);

                }
            }

            curLine = txtBuffer.readLine();
        }

        txtBuffer.close();

        for (Map.Entry<String, Integer> entry : mapPalavras.entrySet()) {

            if (entry.getValue() > 1) {

                for (int i = 0; i < (entry.getValue()); i++) {

                    System.out.println(entry.getKey());
                    bw.append(entry.getKey());
                    bw.append(" ");

                }

            }

        }

        bw.close();

    }

}
